/* $Id$ */

/*****************************************************************************

NAME:
datastore_tdb.c -- implements the datastore, using  tdb.

AUTHORS:
Gyepi Sam <gyepi@praxis-sw.com>   2003

******************************************************************************/

#include "common.h"

#include <tdb.h>
#include <stdlib.h>

#include <errno.h>

#include "datastore.h"
#include "datastore_db.h"
#include "maint.h"
#include "error.h"
#include "xmalloc.h"
#include "xstrdup.h"
#include "paths.h"

typedef struct {
    char *path;
    char *name;
    bool locked;
    bool created;
    TDB_CONTEXT *dbp;
} dbh_t;

/* Function definitions */

const char *db_version_str(void)
{
    return "TrivialDB";
}

static dbh_t *dbh_init(bfpath *bfp)
{
    dbh_t *handle;

    handle = xmalloc(sizeof(dbh_t));
    memset(handle, 0, sizeof(dbh_t));	/* valgrind */

    handle->name = xstrdup(bfp->filepath);

    handle->locked  = false;
    handle->created = false;

    return handle;
}


static void dbh_free(/*@only@*/ dbh_t *handle)
{
    if (handle != NULL) {
	xfree(handle->name);
	xfree(handle->path);
	xfree(handle);
    }
    return;
}


/* Returns is_swapped flag */
bool db_is_swapped(void *vhandle)
{
    (void) vhandle;		/* suppress compiler warning */
    return false;
}


/* Returns created flag */
bool db_created(void *vhandle)
{
    dbh_t *handle = vhandle;
    return handle->created;
}


/*
  Initialize database.
  Returns: pointer to database handle on success, NULL otherwise.
*/
void *db_open(void *dummy, bfpath *bfp, dbmode_t open_mode)
{
    dbh_t *handle;

    int tdb_flags;
    int open_flags;
    TDB_CONTEXT *dbp;

    (void)dummy;

    if (open_mode & DS_WRITE) {
	open_flags = O_RDWR;
	tdb_flags = 0;
    }
    else {
    	open_flags = O_RDONLY;
    	tdb_flags = TDB_NOLOCK;
    }

    handle = dbh_init(bfp);

    if (handle == NULL) return NULL;

    dbp = handle->dbp = tdb_open(handle->name, 0, tdb_flags, open_flags, DS_MODE);

    if ((dbp == NULL) && (open_mode & DS_WRITE)) {
	dbp = handle->dbp = tdb_open(handle->name, 0, tdb_flags, open_flags | O_CREAT, DS_MODE);
	if (dbp != NULL)
	    handle->created = true;
    }

    if (dbp == NULL)
	goto open_err;

    if (DEBUG_DATABASE(1))
	fprintf(dbgout, "(db) tdb_open( %s ), %d )\n", handle->name, open_mode);
      
    if (open_mode & DS_WRITE) {
	if (tdb_lockall(dbp) == 0) {
	    handle->locked = 1;
	}
	else {
	    print_error(__FILE__, __LINE__, "(db) tdb_lockall on file (%s) failed with error %s.",
			handle->name, tdb_errorstr(dbp));
	    goto open_err;
	}
    }

    return handle;

 open_err:
    dbh_free(handle);

    return NULL;
}

int db_delete(void *vhandle, const dbv_t *token)
{
    int ret;
    dbh_t *handle = vhandle;
    TDB_DATA db_key;
    TDB_CONTEXT *dbp;

    db_key.dptr = token->data;
    db_key.dsize = token->leng;

    dbp = handle->dbp;
    ret = tdb_delete(dbp, db_key);

    if (ret != 0) {
	print_error(__FILE__, __LINE__, "(db) tdb_delete('%.*s'), err: %d, %s",
		    CLAMP_INT_MAX(token->leng), (char *)token->data,
		    ret, tdb_errorstr(dbp));
	exit(EX_ERROR);
    }

    return ret;		/* 0 if ok */
}

int db_get_dbvalue(void *vhandle, const dbv_t *token, /*@out@*/ dbv_t *val)
{
    TDB_DATA db_key;
    TDB_DATA db_data;

    dbh_t *handle = vhandle;
    TDB_CONTEXT *dbp = handle->dbp;

    db_key.dptr = token->data;
    db_key.dsize = token->leng;

    db_data = tdb_fetch(dbp, db_key);

    if (db_data.dptr == NULL)
	return DS_NOTFOUND;

    if (val->leng < db_data.dsize) {
	print_error(__FILE__, __LINE__, "(db) db_get_dbvalue( '%.*s' ), size error %lu::%lu",
		    CLAMP_INT_MAX(token->leng), (char *)token->data,
		    (unsigned long)val->leng,
		    (unsigned long)db_data.dsize);
	exit(EX_ERROR);
    }

    val->leng = db_data.dsize;		/* read count */
    memcpy(val->data, db_data.dptr, db_data.dsize);

    return 0;
}


int db_set_dbvalue(void *vhandle, const dbv_t *token, const dbv_t *val)
{
    int ret;
    TDB_DATA db_key;
    TDB_DATA db_data;

    dbh_t *handle = vhandle;
    TDB_CONTEXT *dbp = handle->dbp;

    db_key.dptr = token->data;
    db_key.dsize = token->leng;

    db_data.dptr = val->data;
    db_data.dsize = val->leng;		/* write count */

    ret = tdb_store(dbp, db_key, db_data, TDB_REPLACE);

    if (ret != 0) {
	print_error(__FILE__, __LINE__, "(db) db_set_dbvalue( '%.*s' ), err: %d, %s",
		    CLAMP_INT_MAX(token->leng), (char *)token->data, ret, tdb_errorstr(dbp));
	exit(EX_ERROR);
    }

    return ret;
}


/* Close files and clean up. */
void db_close(void *vhandle)
{
    int ret;
    dbh_t *handle = vhandle;

    if (handle == NULL) return;

    if (DEBUG_DATABASE(1))
	fprintf(dbgout, "db_close (%s)\n", handle->name);

    if (handle->locked) {
	tdb_unlockall(handle->dbp);
    }

    if ((ret = tdb_close(handle->dbp))) {
	print_error(__FILE__, __LINE__, "(db) tdb_close on file %s failed with error %s",
		    handle->path, tdb_errorstr(handle->dbp));
    }

    dbh_free(handle);
}

/*
 flush any data in memory to disk
*/
void db_flush(/*@unused@*/  __attribute__ ((unused)) void *vhandle)
{
    /* noop */
}

typedef struct userdata_t {
    db_foreach_t hook;
    void *userdata;
} userdata_t;


static int tdb_traversor(/*@unused@*/ __attribute__ ((unused)) TDB_CONTEXT * tdb_handle,
                         TDB_DATA key, TDB_DATA data, void *userdata)
{
    int rc;
    dbv_t dbv_key, dbv_data;
    userdata_t *hookdata = userdata;

    /* TDB happily returns data from odd addresses, so we need to
     * memcpy() everything to properly aligned storage (malloc() is
     * fine) if our callee wishes to perform word-sized access - without
     * memcpy(), the callee will die with SIGBUS (SPARC, m68k) or get
     * extremely slow (i386). */

    /* XXX FIXME: Possible optimization if this function is only used by
     * one caller at a time (i. e. no threads): allocate buffers
     * statically and reuse them as long as they are of sufficient size
     * and reallocate otherwise. */

    /* copy key */
    /* XXX FIXME: do we really need to use C-string compatible keys?
     * Looks wasteful. */
    dbv_key.leng = key.dsize;
    dbv_key.data = xmalloc(dbv_key.leng+1);
    memcpy(dbv_key.data, key.dptr, dbv_key.leng);
    ((char *)dbv_key.data)[dbv_key.leng] = '\0';

    /* copy data */
    dbv_data.leng = data.dsize;
    dbv_data.data = xmalloc(dbv_data.leng);
    memcpy(dbv_data.data, data.dptr, dbv_data.leng);

    /* call user function */
    rc = hookdata->hook(&dbv_key, &dbv_data, hookdata->userdata);

    xfree(dbv_data.data);
    xfree(dbv_key.data);

    return rc;
}


ex_t db_foreach(void *vhandle, db_foreach_t hook, void *userdata)
{
    int ret;
    dbh_t *handle = vhandle;
    TDB_CONTEXT *dbp = handle->dbp;

    userdata_t hookdata;

    hookdata.hook = hook;
    hookdata.userdata = userdata;

    ret = tdb_traverse(dbp, tdb_traversor, (void *) &hookdata);
    if (ret == -1) {
	print_error(__FILE__, __LINE__, "(db) db_foreach err: %d, %s",
		    ret, tdb_errorstr(dbp));
	exit(EX_ERROR);
    }

    return EX_OK;
}

/*
 * The following struct and function were taken from tdb-1.0.6
 *
 * Samba database functions
 * Copyright (C) Andrew Tridgell              1999-2000
 * Copyright (C) Luke Kenneth Casson Leighton      2000
 * Copyright (C) Paul 'Rusty' Russell              2000
 * Copyright (C) Jeremy Allison                    2000
 *
 * This code was licensed under conditions of the GPL v2 or later.
 */

static struct tdb_errname {
    enum TDB_ERROR ecode; 
    const char *estring;
} emap[] = { 
    {TDB_SUCCESS,     "Success"},
    {TDB_ERR_CORRUPT, "Corrupt database"},
    {TDB_ERR_IO,      "IO Error"},
    {TDB_ERR_LOCK,    "Locking error"},
    {TDB_ERR_OOM,     "Out of memory"},
    {TDB_ERR_EXISTS,  "Record exists"},
    {TDB_ERR_NOLOCK,  "Lock exists on other keys"},
    {TDB_ERR_NOEXIST, "Record does not exist"}
};

/* Error string for the last tdb error */
const char *db_str_err(int j)
{
	uint32_t i;
	enum TDB_ERROR e = j;

	for (i = 0; i < sizeof(emap) / sizeof(struct tdb_errname); i++)
		if (e == emap[i].ecode)
			return emap[i].estring;
	return "Invalid error code";
}
