/**
 * \file datastore_sqlite.c SQLite 3 database driver back-end
 * \author Matthias Andree <matthias.andree@gmx.de>
 * \date 2004
 *
 * \bug does not expose the UPDATE command for fast modification
 *
 * This file handles a static table named "bogofilter" in a SQLite3
 * database. The table has two "BLOB"-typed columns, key and value.
 *
 * GNU GENERAL PUBLIC LICENSE v2
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

/** Structure to hold database handle and associated data. */
struct dbhsqlite_t {
    char *path;	   /**< directory to hold database */
    char *name;	   /**< database file name */
    sqlite3 *db;   /**< pointer to SQLite3 handle */
    bool created;  /**< flag, gets set by db_open on creation of the database */
};
/** Convenience shortcut to avoid typing "struct dbh_t" */
typedef struct dbhsqlite_t dbh_t;

/* dummy functions */
#define DUMMYVVP(name)  void name(void *dummy) { (void)dummy; }
#define DUMMYVPVP(name) void *name(void *dummy) { (void)dummy; return NULL; }
#define DUMMYICP(name)  ex_t name(const char *dummy) { (void)dummy; return EX_OK; }
DUMMYVVP(dbe_cleanup)
DUMMYVVP(db_flush)
DUMMYVPVP(db_get_env)
DUMMYICP(db_verify)
DUMMYICP(dbe_purgelogs)
DUMMYICP(dbe_remove)
void *dbe_init(const char *dummy) { (void)dummy; return (void *)~0; }
ex_t dbe_recover(const char *d1, bool d2, bool d3) { (void)d1; d2=d3; return EX_ERROR; }
bool db_is_swapped(void *dummy) { (void)dummy; return false; }

/** The layout of the bogofilter table, formatted as SQL statement. */
#define LAYOUT \
	"CREATE TABLE bogofilter (" \
	"   key   BLOB PRIMARY KEY, "\
	"   value BLOB);"

#define BEGIN \
	"BEGIN TRANSACTION;"

/* real functions */
/** Initialize database handle and return it. 
 * \returns non-NULL, as it exits with EX_ERROR in case of trouble. */
static dbh_t *handle_init(
	const char *path /** directory to database file */,
	const char *name /** name of database file */)
{
    dbh_t *handle;
    size_t len = strlen(path) + strlen(name) + 2;

    handle = xmalloc(sizeof(dbh_t));
    memset(handle, 0, sizeof(dbh_t));

    handle->path = xstrdup(path);
    handle->name = xmalloc(len);
    build_path(handle->name, len, path, name);

    return handle;
}

/** Free internal database handle \a dbh. */
static void free_dbh(dbh_t *dbh) {
    if (!dbh)
	return;
    free(dbh->name);
    free(dbh->path);
    free(dbh);
}

/** Executes the SQL statement \a cmd on the database \a db and returns
 * the sqlite3_exec return code, except SQLITE_BUSY which is handled
 * internally by sleeping and retrying. If the return code is nonzero, this
 * routine will have printed an error message.
 */
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

/** Short trace handler function, passed to SQLite if debugging is
 * enabled. */
static void db_trace(void *userdata /** unused */,
	const char *log /** log message */) {
    (void)userdata;
    fprintf(dbgout, "SQLite[%ld]: %s\n", (long)getpid(), log);
}

/** Callback function that increates the int variable that \a userdata
 * points to, for db_loop. */
static ex_t db_count(dbv_t *d1, dbv_t *d2, void *userdata) {
    int *counter = userdata;
    ++ *counter;
    (void)d1;
    (void)d2;
    return 0;
}

/** Dual-mode get/foreach function, if \a hook is NULL, we do a get and
 * treat \a userdata as a dbv_t, if \a hook is non-NULL, we call it for
 * each (key, value) tuple in the database.
 */
static int db_loop(sqlite3 *db,	/**< SQLite3 database handle */
	const char *cmd,	/**< SQL command to obtain data */
	db_foreach_t hook,	/**< if non-NULL, called for each value */
	void *userdata		/**  if \a hook is NULL, this is our output dbv_t pointer,
				 *   if \a hook is non-NULL, this is
				 *   passed to the \a hook
				 */
	) {
    const char *tail;
    sqlite3_stmt *stmt;
    int rc;
    bool loop, found;
    found = false;
    /* sqlite3_exec doesn't allow us to retrieve BLOBs */
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
		    /* get mode - read value from first row, then return */
		    dbv_t *val = userdata;

		    loop = false;
		    val->leng = sqlite3_column_bytes(stmt, /* column */ 0);
		    val->data = xmalloc(val->leng);
		    memcpy(val->data, sqlite3_column_blob(stmt, 0), val->leng);
		} else {
		    /* foreach mode - call hook for each (key, value)
		     * tuple */
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
    /* free resources */
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
	/* using IMMEDIATE or DEFERRED here locks up in t.lock3
	 * or t.bulkmode */
	if (sqlexec(dbh->db, "BEGIN EXCLUSIVE TRANSACTION;")) goto barf;
	/*
	 * trick: the sqlite_master table (see SQLite FAQ) is read-only
	 * and lists all table, indexes etc. so we use it to check if
	 * the bogofilter table is already there, the error codes are
	 * too vague either way, for "no such table" and "table already
	 * exists" we always get SQLITE_ERROR, which we'll also get for
	 * syntax errors, such as "EXCLUSIVE" not supported on older
	 * versions :-(
	 */
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
/** Formats \a cmd and following arguments with sqlite3_vmprintf (hence
 * supporting %%q for SQL-quoted strings) and executes it using sqlexec.
 */
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

/** Converts \a len unsigned characters starting at \a input into the
 * SQL X'b1a4' notation, returns malloc'd string that the caller must
 * free. */
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

int db_get_dbvalue(void *vhandle, const dbv_t* key, /*@out@*/ dbv_t *val) {
    dbh_t *dbh = vhandle;
    char *k = binenc(key->data, key->leng);
    char *cmd = sqlite3_mprintf("SELECT value FROM bogofilter "
				"WHERE(key = %s) LIMIT 1;", k);
    int rc = db_loop(dbh->db, cmd, NULL, val);
    sqlite3_free(cmd);
    xfree(k);
    return rc;
}

ex_t db_foreach(void *vhandle, db_foreach_t hook, void *userdata) {
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
