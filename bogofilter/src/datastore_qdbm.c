/* $Id$ */

/*****************************************************************************

NAME:
datastore_qdbm.c -- implements the datastore, using qdbm.

AUTHORS:
Gyepi Sam <gyepi@praxis-sw.com>          2003
Matthias Andree <matthias.andree@gmx.de> 2003
Stefan Bellon <sbellon@sbellon.de>       2003

******************************************************************************/

#include "common.h"

#include <depot.h>
#include <stdlib.h>

#include "datastore.h"
#include "datastore_db.h"
#include "error.h"
#include "paths.h"
#include "xmalloc.h"
#include "xstrdup.h"

/* initial bucket array element count (for new data base) */
static const int DB_INITBNUM = 1913;

/* align to size for quick overwrites. */
static const int DB_ALIGNSIZE = 12;

/* reorganize data base if record/bucket ratio grows above this 
 * hence "FILL" for "bucket fill ratio". */
static const double DB_MAXFILL = 0.8;

typedef struct {
    char *path;
    char *name;
    bool locked;
    DEPOT *dbp;
} dbh_t;


/* Function definitions */

const char *db_version_str(void)
{
    static char v[80];
    if (!v[0])
	snprintf(v, sizeof(v), "QDBM (version %s, Depot API)", dpversion);
    return v;
}


static dbh_t *dbh_init(const char *path, const char *name)
{
    dbh_t *handle;
    size_t len = strlen(path) + strlen(name) + 2;

    handle = xmalloc(sizeof(dbh_t));
    memset(handle, 0, sizeof(dbh_t));	/* valgrind */

    handle->path = xstrdup(path);

    handle->name = xmalloc(len);
    build_path(handle->name, len, path, name);

    handle->locked = false;

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


/*
  Initialize database.
  Returns: pointer to database handle on success, NULL otherwise.
*/
void *db_open(const char *db_file, const char *name, dbmode_t open_mode)
{
    dsh_t *dsh;
    dbh_t *handle;

    int flags;
    DEPOT *dbp;

    if (open_mode == DB_WRITE)
	flags = DP_OWRITER | DP_OCREAT;
    else
	flags = DP_OREADER;

    handle = dbh_init(db_file, name);

    if (handle == NULL) return NULL;

    dsh = dsh_init(handle, false);

    dbp = handle->dbp = dpopen(handle->name, flags, DB_INITBNUM);

    if (dbp == NULL)
	goto open_err;

    if (flags & DP_OWRITER) {
	if (!dpsetalign(dbp, DB_ALIGNSIZE)){
	    dpclose(dbp);
	    goto open_err;
	}
    }

    if (DEBUG_DATABASE(1))
	fprintf(dbgout, "(qdbm) dpopen( %s, %d )\n", handle->name, open_mode);

    return dsh;

 open_err:
    print_error(__FILE__, __LINE__, "(qdbm) dpopen(%s, %d) failed: %s",
		handle->name, flags, dperrmsg(dpecode));
    dbh_free(handle);

    return NULL;
}


int db_delete(dsh_t *dsh, const dbv_t *token)
{
    int ret;
    dbh_t *handle = dsh->dbh;
    DEPOT *dbp;

    dbp = handle->dbp;
    ret = dpout(dbp, token->data, token->leng);

    if (ret == 0) {
	print_error(__FILE__, __LINE__, "(qdbm) dpout('%.*s'), err: %s",
		    CLAMP_INT_MAX(token->leng),
		    (char *)token->data, dperrmsg(dpecode));
	exit(EX_ERROR);
    }
    ret = ret ^ 1;	/* ok is 1 in qdbm and 0 in bogofilter */

    return ret;		/* 0 if ok */
}


int db_get_dbvalue(dsh_t *dsh, const dbv_t *token, /*@out@*/ dbv_t *val)
{
    char *data;
    int dsiz;

    dbh_t *handle = dsh->dbh;
    DEPOT *dbp = handle->dbp;

    data = dpget(dbp, token->data, token->leng, 0, -1, &dsiz);

    if (data == NULL)
	return DS_NOTFOUND;

    if (val->leng < (unsigned)dsiz) {
	print_error(__FILE__, __LINE__,
		    "(qdbm) db_get_dbvalue( '%.*s' ), size error %lu: %lu",
		    CLAMP_INT_MAX(token->leng),
		    (char *)token->data, (unsigned long)val->leng,
		    (unsigned long)dsiz);
	exit(EX_ERROR);
    }

    val->leng = dsiz;		/* read count */
    memcpy(val->data, data, dsiz);

    free(data); /* not xfree() as allocated by dpget() */

    return 0;
}


/*
   Re-organize database when fill ratio > DB_MAXFILL
*/
static inline void db_optimize(DEPOT *dbp, char *name)
{
    int bnum = dpbnum(dbp); /* very cheap: O(1) */
    int rnum = dprnum(dbp); /* very cheap: O(1) */
    if (bnum > 0 && rnum > 0 && ((double)rnum / bnum > DB_MAXFILL)) {
	if (!dpoptimize(dbp, -1))
	    print_error(__FILE__, __LINE__,
			"(qdbm) dpoptimize for %s failed: %s",
			name, dperrmsg(dpecode));
    }
}


int db_set_dbvalue(dsh_t *dsh, const dbv_t *token, dbv_t *val)
{
    int ret;
    dbh_t *handle = dsh->dbh;
    DEPOT *dbp = handle->dbp;

    ret = dpput(dbp, token->data, token->leng, val->data, val->leng, DP_DOVER);

    if (ret == 0) {
	print_error(__FILE__, __LINE__,
		    "(qdbm) db_set_dbvalue( '%.*s' ) failed: %s",
		    CLAMP_INT_MAX(token->leng), (char *)token->data,
		    dperrmsg(dpecode));
	exit(EX_ERROR);
    }

    db_optimize(dbp, handle->name);

    return 0;
}


/*
   Close files and clean up.
*/
void db_close(void *vhandle, bool nosync)
{
    dbh_t *handle = vhandle;
    DEPOT *dbp;

    if (handle == NULL) return;

    if (DEBUG_DATABASE(1))
	fprintf(dbgout, "(qdbm) dpclose(%s, %s)\n", handle->name, nosync ? "nosync" : "sync");

    dbp = handle->dbp;

    db_optimize(dbp, handle->name);

    if (!dpclose(dbp))
	print_error(__FILE__, __LINE__, "(qdbm) dpclose for %s failed: %s",
		    handle->name, dperrmsg(dpecode));

    handle->dbp = NULL;

    dbh_free(handle);
}


/*
   Flush any data in memory to disk
*/
void db_flush(dsh_t *dsh)
{
    dbh_t *handle = dsh->dbh;
    DEPOT * dbp = handle->dbp;

    if (!dpsync(dbp))
	print_error(__FILE__, __LINE__, "(qdbm) dpsync failed: %s",
		    dperrmsg(dpecode));
}


int db_foreach(dsh_t *dsh, db_foreach_t hook, void *userdata)
{
    int ret = 0;

    dbh_t *handle = dsh->dbh;
    DEPOT *dbp = handle->dbp;

    dbv_t dbv_key, dbv_data;
    int ksiz, dsiz;
    char *key, *data;

    ret = dpiterinit(dbp);
    if (ret) {
	while ((key = dpiternext(dbp, &ksiz))) {
	    data = dpget(dbp, key, ksiz, 0, -1, &dsiz);
	    if (data) {
		/* switch to "dbv_t *" variables */
		dbv_key.leng = ksiz;
		dbv_key.data = xmalloc(dbv_key.leng+1);
		memcpy(dbv_key.data, key, ksiz);
		((char *)dbv_key.data)[dbv_key.leng] = '\0';

		dbv_data.data = data;
		dbv_data.leng = dsiz;		/* read count */

		/* call user function */
		ret = hook(&dbv_key, &dbv_data, userdata);

		xfree(dbv_key.data);

		if (ret != 0)
		    break;
		free(data); /* not xfree() as allocated by dpget() */
	    }
	    free(key); /* not xfree() as allocated by dpiternext() */
	}
    } else {
	print_error(__FILE__, __LINE__, "(qdbm) dpiterinit err: %s",
		    dperrmsg(dpecode));
	exit(EX_ERROR);
    }

    return 0;
}


const char *db_str_err(int e) {
    return dperrmsg(e);
}

/* dummy infrastructure, to be expanded by environment
 * or transactional initialization/shutdown */
static bool init = false;
int db_init(void) { init = true; return 0; }
void db_cleanup(void) { init = false; }
