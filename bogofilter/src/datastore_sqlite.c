/**
 * \file datastore_sqlite.c SQLite 3 database driver back-end
 * \author Matthias Andree <matthias.andree@gmx.de>
 * \date 2004, 2005
 *
 * This file handles a static table named "bogofilter" in a SQLite3
 * database. The table has two "BLOB"-typed columns, key and value.
 *
 * GNU GENERAL PUBLIC LICENSE v2
 */

#include "common.h"

#include <errno.h>
#include <unistd.h>		/* for getpid() */
#include <sqlite3.h>

#include "datastore_db.h"

#include "error.h"
#include "rand_sleep.h"
#include "xmalloc.h"
#include "xstrdup.h"

/** Structure to hold database handle and associated data. */
struct dbhsqlite_t {
    char *path;	   /**< directory to hold database */
    char *name;	   /**< database file name */
    sqlite3 *db;   /**< pointer to SQLite3 handle */
    bool created;  /**< gets set by db_open if it created the database new */
    bool swapped;  /**< if endian swapped on disk vs. current host */
};

/** Convenience shortcut to avoid typing "struct dbh_t" */
typedef struct dbhsqlite_t dbh_t;

static const char *ENDIAN32 = ".ENDIAN32";

void db_flush(void *unused) { (void)unused; }

static int sql_txn_begin(void *vhandle);
static int sql_txn_abort(void *vhandle);
static int sql_txn_commit(void *vhandle);
static u_int32_t sql_pagesize(bfpath *bfp);

/** The layout of the bogofilter table, formatted as SQL statement. */
#define LAYOUT \
	"CREATE TABLE bogofilter (" \
	"   key   BLOB PRIMARY KEY, "\
	"   value BLOB);"

dsm_t dsm_sqlite = {
    /* public -- used in datastore.c */
    &sql_txn_begin,
    &sql_txn_abort,
    &sql_txn_commit,

    /* private -- used in datastore_db_*.c */
    NULL,	/* dsm_env_init         */
    NULL,	/* dsm_cleanup          */
    NULL,	/* dsm_cleanup_lite     */
    NULL,	/* dsm_get_env_dbe      */
    NULL,	/* dsm_database_name    */
    NULL,	/* dsm_recover_open     */
    NULL,	/* dsm_auto_commit_flags*/                    
    NULL,	/* dsm_get_rmw_flag     */
    NULL,	/* dsm_lock             */
    NULL,	/* dsm_common_close     */
    NULL,	/* dsm_sync             */
    NULL,	/* dsm_log_flush        */
    &sql_pagesize,/* dsm_pagesize       */
    NULL,	/* dsm_purgelogs        */
    NULL,	/* dsm_checkpoint       */
    NULL,	/* dsm_recover          */
    NULL,	/* dsm_remove           */
    NULL	/* dsm_verify           */
};

dsm_t *dsm = &dsm_sqlite;

/** The command to begin a regular transaction. */
#define BEGIN \
	"BEGIN TRANSACTION;"

/* real functions */
/** Initialize database handle and return it. 
 * \returns non-NULL, as it exits with EX_ERROR in case of trouble. */
static dbh_t *dbh_init(bfpath *bfp)
{
    dbh_t *handle;

    dsm = &dsm_sqlite;

    handle = xmalloc(sizeof(dbh_t));
    memset(handle, 0, sizeof(dbh_t));

    handle->name = xstrdup(bfp->filepath);

    return handle;
}

/** Free internal database handle \a dbh. */
static void free_dbh(dbh_t *dbh) {
    if (!dbh)
	return;
    xfree(dbh->name);
    xfree(dbh->path);
    xfree(dbh);
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
	if (rc == SQLITE_BUSY) {
	    if (DEBUG_DATABASE(2))
		fprintf(dbgout, "database busy, sleeping and retrying...\n");
	    rand_sleep(1000, 100000);
	} else {
	    break;
	}
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
    do {
	rc = sqlite3_prepare(db, cmd, strlen(cmd), &stmt, &tail);
	if (rc == SQLITE_BUSY) {
	    if (stmt)
		sqlite3_finalize(stmt);
	    if (DEBUG_DATABASE(2))
		fprintf(dbgout, "database busy, sleeping and retrying...\n");
	    rand_sleep(1000, 100000);
	}
    } while (rc == SQLITE_BUSY);
    if (rc) {
	print_error(__FILE__, __LINE__,
		"Error preparing \"%s\": %s (#%d)\n",
		cmd, sqlite3_errmsg(db), rc);
	sqlite3_finalize(stmt);
	return rc;
    }
    loop = true;
    while(loop) {
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
		    if (key.leng != strlen(ENDIAN32)
			    || memcmp(key.data, ENDIAN32, key.leng) != 0)
			rc = hook(&key, &val, userdata);
		    xfree(val.data);
		    xfree(key.data);
		    if (rc) {
			sqlite3_finalize(stmt);
			return rc;
		    }
		}
		break;
	    case SQLITE_DONE:
		loop = false;
		break;
	    case SQLITE_BUSY:
		if (DEBUG_DATABASE(2))
		    fprintf(dbgout, "database busy, sleeping and retrying...\n");
		rand_sleep(1000, 100000);
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

void *db_open(void *dummyenv, bfpath *bfp, dbmode_t mode)
{
    int rc;
    dbh_t *dbh;
    dbv_t k, v;
    int count = 0;

    (void)dummyenv;

    dbh = dbh_init(bfp);
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
		{
		    u_int32_t p[2] = { 0x01020304, 0x01020304 };
		    int rc2;
		    k.data = xstrdup(ENDIAN32);
		    k.leng = strlen(k.data);
		    v.data = p;
		    v.leng = sizeof(p);
		    if (sqlexec(dbh->db, LAYOUT)) goto barf;
		    rc2 = db_set_dbvalue(dbh, &k, &v);
		    xfree(k.data);
		    if (rc2)
			goto barf;
		    if (sqlexec(dbh->db, "COMMIT;")) goto barf;
		    dbh->created = true;
		}
		break;
	    default:
		goto barf;
	}
    }

    /* check if byteswapped */
    {
	u_int32_t t;
	int ee;
	k.data = xstrdup(ENDIAN32);
	k.leng = strlen(k.data);

	ee = db_get_dbvalue(dbh, &k, &v);
	xfree(k.data);
	switch(ee) {
	    case 0: /* found endian marker token, read it */
		if (v.leng < 4)
		    goto barf;
		t = ((u_int32_t *)v.data)[0];
		xfree(v.data);
		switch (t) {
		    case 0x01020304: /* same endian, "UNIX" */
			dbh->swapped = false;
			break;
		    case 0x04030201: /* swapped, "XINU" */
			dbh->swapped = true;
			break;
		    default: /* NUXI or IXUN or crap */
			print_error(__FILE__, __LINE__,
				"Unknown endianness on %s: %08x.\n",
				dbh->name, ((u_int32_t *)v.data)[0]);
			goto barf2;
		}
		break;
	    case DS_NOTFOUND: /* no marker token, assume not swapped */
		dbh->swapped = false;
		break;
	    default:
		goto barf;
	}
    }

    return dbh;
barf:
    print_error(__FILE__, __LINE__, "Error on database %s: %s\n",
	    dbh->name, sqlite3_errmsg(dbh->db));
barf2:
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
	snprintf(buf, sizeof(buf), "SQLite %s", sqlite3_version);
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

static int sql_txn_begin(void *vhandle) {
    dbh_t *dbh = vhandle;
    return sqlexec(dbh->db,  BEGIN );
}

static int sql_txn_abort(void *vhandle) {
    dbh_t *dbh = vhandle;
    return sqlexec(dbh->db, "ROLLBACK;");
}

static int sql_txn_commit(void *vhandle) {
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

bool db_is_swapped(void *vhandle) {
    dbh_t *dbh = vhandle;
    return dbh->swapped;
}

static int pagesize_cb(void *ptr, int argc, char **argv, char **dummy) {
    u_int32_t *uptr = ptr;

    (void)dummy;

    if (argc != 1)
	return -1;
    errno = 0;
    *uptr = strtoul(argv[0], NULL, 0);
    return errno;
}

static u_int32_t sql_pagesize(bfpath *bfp)
{
    dbh_t *dbh;
    int rc;
    u_int32_t size;

    dbh = db_open(NULL, bfp, DS_READ);
    if (!dbh)
	return 0xffffffff;
    do {
	rc = sqlite3_exec(dbh->db, "PRAGMA page_size;", pagesize_cb, &size, NULL);
	if  (rc == SQLITE_BUSY) rand_sleep(1000, 100000);
    } while (rc == SQLITE_BUSY);
    if (rc != SQLITE_OK) {
	return 0xffffffff;
    }
    db_close(dbh);
    return size;
}
