/* 
 * \file datastore_sqlite.c - SQLite 3 database driver back-end
 * \author Matthias Andree <matthias.andree@gmx.de
 * \year 2004
 *
 * \bugs does not expose the UPDATE command for fast modification
 *
 * LICENSE: GNU GPL v2
 */

#include "config.h"
#include "datastore_db.h"
#include "error.h"
#include "paths.h"
#include "rand_sleep.h"
#include "xmalloc.h"
#include "xstrdup.h"
#include <stdarg.h>
#include <unistd.h> /* for getpid() */
#include <sqlite3.h>

struct dbh_t {
    char *path;
    char *name;
    sqlite3 *db;
    bool created;
};
typedef struct dbh_t dbh_t;

/* dummy functions */
#define DUMMYVVP(name) void name(void *dummy) { (void)dummy; }
#define DUMMYVPVP(name) void *name(void *dummy) { (void)dummy; return NULL; }
#define DUMMYICP(name) int name(const char *dummy) { (void)dummy; return 0; }
DUMMYVVP(dbe_cleanup)
DUMMYVVP(db_flush)
DUMMYVPVP(db_get_env)
DUMMYICP(db_verify)
DUMMYICP(dbe_purgelogs)
DUMMYICP(dbe_remove)
void *dbe_init(const char *dummy) { (void)dummy; return (void *)~0; }
int dbe_recover(const char *d1, bool d2, bool d3) { (void)d1; d2=d3; return 0; }
bool db_is_swapped(void *dummy) { (void)dummy; return false; }

/* TABLE LAYOUT */
#define LAYOUT \
	"CREATE TABLE bogofilter (" \
	"   key   BLOB PRIMARY KEY, "\
	"   value BLOB);"

#define BEGIN \
	"BEGIN TRANSACTION;"

/* real functions */
static dbh_t *handle_init(const char *path, const char *name)
{
    dbh_t *handle;
    size_t len = strlen(path) + strlen(name) + 2;

    handle = xmalloc(sizeof(dbh_t));
    memset(handle, 0, sizeof(dbh_t));   /* valgrind */

    handle->path = xstrdup(path);
    handle->name = xmalloc(len);
    build_path(handle->name, len, path, name);

    return handle;
}

static void free_dbh(dbh_t *dbh) {
    if (!dbh)
	return;
    free(dbh->name);
    free(dbh->path);
    free(dbh);
}

static int sqlexec(sqlite3 *db, const char *cmd) {
    char *e = NULL;
    int rc;
    while(1) {
	rc = sqlite3_exec(db, cmd, NULL, NULL, &e);
	if (rc == SQLITE_BUSY) rand_sleep(1000, 100000);
	else break;
    }
    if (rc) {
	print_error(__FILE__, __LINE__,
		"Error executing \"%s\": %s (#%d)\n",
		cmd, e ? e : "NULL", rc);
	if (e)
	    sqlite3_free(e);
    }
    return rc;
}

static void db_trace(void *userdata, const char *log) {
    (void)userdata;
    fprintf(dbgout, "SQLite[%ld]: %s\n", (long)getpid(), log);
}

static int db_count(dbv_t *d1, dbv_t *d2, void *userdata) {
    int *counter = userdata;
    ++ *counter;
    (void)d1;
    (void)d2;
    return 0;
}

static int db_loop(sqlite3 *db, const char *cmd, db_foreach_t hook,
	void *userdata) {
    const char *tail;
    sqlite3_stmt *stmt;
    int rc;
    bool loop, found;
    found = false;
    rc = sqlite3_prepare(db, cmd, strlen(cmd), &stmt, &tail);
    if (rc) {
	print_error(__FILE__, __LINE__,
		"Error preparing \"%s\": %s (#%d)\n",
		cmd, sqlite3_errmsg(db), rc);
	sqlite3_finalize(stmt);
	return rc;
    }
    for(loop = true; loop;) {
	switch(sqlite3_step(stmt)) {
	    case SQLITE_ROW:
		found = true;
		if (hook == NULL) {
		    /* get mode */
		    dbv_t *val = userdata;

		    loop = false;
		    val->leng = sqlite3_column_bytes(stmt, /* column */ 0);
		    val->data = xmalloc(val->leng);
		    memcpy(val->data, sqlite3_column_blob(stmt, 0), val->leng);
		} else {
		    /* foreach mode */
		    dbv_t key, val;

		    key.leng = sqlite3_column_bytes(stmt, /* column */ 0);
		    val.leng = sqlite3_column_bytes(stmt, /* column */ 1);
		    key.data = xmalloc(key.leng);
		    val.data = xmalloc(val.leng);
		    memcpy(key.data, sqlite3_column_blob(stmt, 0), key.leng);
		    memcpy(val.data, sqlite3_column_blob(stmt, 1), val.leng);
		    rc = hook(&key, &val, userdata);
		    free(val.data);
		    free(key.data);
		    if (rc) {
			sqlite3_finalize(stmt);
			return rc;
		    }
		}
		break;
	    case SQLITE_DONE:
		loop = false;
		break;
	    default:
		print_error(__FILE__, __LINE__, "Error executing \"%s\": %s (#%d)\n",
		cmd, sqlite3_errmsg(db), rc);
		sqlite3_finalize(stmt);
		return rc;
	}
    }
    sqlite3_finalize(stmt);
    return found ? 0 : DS_NOTFOUND;
}

void *db_open(void *dummyenv, const char *dbpath,
	const char *dbname, dbmode_t mode) {
    int rc;
    dbh_t *dbh;
    int count = 0;

    (void)dummyenv;
    dbh = handle_init(dbpath, dbname);
    if (DEBUG_DATABASE(1) || getenv("BF_DEBUG_DB"))
	fprintf(dbgout, "SQLite: db_open(%s)\n", dbh->name);
    rc = sqlite3_open(dbh->name, &dbh->db);
    if (rc) {
	print_error(__FILE__, __LINE__, "Can't open database %s: %s\n",
		dbh->name, sqlite3_errmsg(dbh->db));
	goto barf;
    }

    if (DEBUG_DATABASE(1) || getenv("BF_DEBUG_DB"))
	sqlite3_trace(dbh->db, db_trace, NULL);

    if (mode != DS_READ) {
	if (sqlexec(dbh->db, "BEGIN EXCLUSIVE TRANSACTION;")) goto barf;
	rc = db_loop(dbh->db, "SELECT name FROM sqlite_master "
		"WHERE type='table' AND name='bogofilter';",
		db_count, &count);
	switch(rc) {
	    case 0:
		if (sqlexec(dbh->db, "COMMIT;")) goto barf;
		break;
	    case DS_NOTFOUND:
		if (sqlexec(dbh->db, LAYOUT)) goto barf;
		if (sqlexec(dbh->db, "COMMIT;")) goto barf;
		dbh->created = true;
		break;
	    default:
		goto barf;
	}
    }

    return dbh;
barf:
    print_error(__FILE__, __LINE__, "Error on database %s: %s\n",
	    dbh->name, sqlite3_errmsg(dbh->db));
    sqlite3_close(dbh->db);
    free_dbh(dbh);
    return NULL;
}

void db_close(void *handle) {
    int rc;
    dbh_t *dbh = handle;
    rc = sqlite3_close(dbh->db);
    if (rc) {
	print_error(__FILE__, __LINE__, "Can't close database %s: %d",
		dbh->name, rc);
	exit(EX_ERROR);
    }
    free_dbh(dbh);
}

const char *db_version_str(void) {
    static char buf[80];
    if (!buf[0])
	snprintf(buf, sizeof(buf), "SQLite %s", sqlite3_libversion());
    return buf;
}

static int sqlfexec(sqlite3 *db, const char *cmd, ...)
    __attribute__ ((format(printf,2,3)));
static int sqlfexec(sqlite3 *db, const char *cmd, ...)
{
    char *buf;
    va_list ap;
    int rc;

    va_start(ap, cmd);
    buf = sqlite3_vmprintf(cmd, ap);
    va_end(ap);
    rc = sqlexec(db, buf);
    free(buf);
    return rc;
}

int db_txn_begin(void *vhandle) {
    dbh_t *dbh = vhandle;
    return sqlexec(dbh->db, BEGIN);
}

int db_txn_abort(void *vhandle) {
    dbh_t *dbh = vhandle;
    return sqlexec(dbh->db, "ROLLBACK;");
}

int db_txn_commit(void *vhandle) {
    dbh_t *dbh = vhandle;
    return sqlexec(dbh->db, "COMMIT;");
}

/* allcoates memory and converts len bytes starting at in
 * into a HEX notation string, X'FCE2' or similar */
static char *binenc(const void *input, size_t len) {
    const unsigned char *in = input;
    const char hexdig[] = "0123456789ABCDEF";
    char *out = xmalloc(4 + len * 2);
    char *t = out;
    size_t i;

    *t++ = 'X';
    *t++ = '\'';
    for(i = 0; i < len; i++) {
	*t++ = hexdig[in[i] >> 4];
	*t++ = hexdig[in[i] & 0xf];
    }

    *t++ = '\'';
    *t++ = '\0';
    return out;
}

int db_delete(void *vhandle, const dbv_t *key) {
    dbh_t *dbh = vhandle;
    int rc;
    char *e = binenc(key->data, key->leng);
    rc = sqlfexec(dbh->db, "DELETE FROM bogofilter WHERE(key = %s);", e);
    xfree(e);
    return rc;
}

int db_set_dbvalue(void *vhandle, const dbv_t *key, const dbv_t *val) {
    dbh_t *dbh = vhandle;
    int rc;
    char *k = binenc(key->data, key->leng);
    char *v = binenc(val->data, val->leng);
    rc = sqlfexec(dbh->db, "INSERT OR REPLACE INTO bogofilter "
	    "VALUES(%s,%s);", k, v);
    xfree(k);
    xfree(v);
    return rc;
}

/* This is a dual-function get/foreach function.
 * If hook is NULL, it has get functionality, treating userdata as dbv_t
 * and filling it in.
 * If hook is non-NULL, it has foreach fucntionality.
 */
int db_get_dbvalue(void *vhandle, const dbv_t* key, /*@out@*/ dbv_t *val) {
    dbh_t *dbh = vhandle;
    char *k = binenc(key->data, key->leng);
    char *cmd = sqlite3_mprintf("SELECT value FROM bogofilter WHERE(key = %s) LIMIT 1;", k);
    int rc = db_loop(dbh->db, cmd, NULL, val);
    sqlite3_free(cmd);
    xfree(k);
    return rc;
}

int db_foreach(void *vhandle, db_foreach_t hook, void *userdata) {
    dbh_t *dbh = vhandle;
    const char *cmd = "SELECT key, value FROM bogofilter;";
    return db_loop(dbh->db, cmd, hook, userdata);
}

const char *db_str_err(int e) {
    return e == 0 ? "no error" : "unknown condition (not yet implemented)";
}

bool db_created(void *vhandle) {
    dbh_t *dbh = vhandle;
    return dbh->created;
}
