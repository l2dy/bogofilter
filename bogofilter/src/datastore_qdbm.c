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
    size_t count;
    char *name[IX_SIZE];
    bool locked;
    DEPOT *dbp[IX_SIZE];
} dbh_t;


/* Function definitions */

const char *db_version_str(void)
{
    static char v[80];
    if (!v[0])
	snprintf(v, sizeof(v), "QDBM (version %s, Depot API)", dpversion);
    return v;
}


static dbh_t *dbh_init(const char *path, size_t count, const char **names)
{
    size_t c;
    dbh_t *handle;

    handle = xmalloc(sizeof(dbh_t));
    memset(handle, 0, sizeof(dbh_t));	/* valgrind */

    handle->count = count;
    handle->path = xstrdup(path);
    for (c = 0; c < count; c += 1) {
	size_t len = strlen(path) + strlen(names[c]) + 2;
	handle->name[c] = xmalloc(len);
	build_path(handle->name[c], len, path, names[c]);
    }
    handle->locked = false;

    return handle;
}


static void dbh_free(/*@only@*/ dbh_t *handle)
{
    if (handle != NULL) {
      size_t c;
      for (c = 0; c < handle->count; c += 1)
	  xfree(handle->name[c]);

      xfree(handle->path);
      xfree(handle);
    }
    return;
}


static void dbh_print_names(dbh_t *handle, const char *msg)
{
    if (handle->count == 1)
	fprintf(dbgout, "%s (%s)", msg, handle->name[0]);
    else
	fprintf(dbgout, "%s (%s,%s)", msg, handle->name[0], handle->name[1]);
}


/*
  Initialize database.
  Returns: pointer to database handle on success, NULL otherwise.
*/
void *db_open(const char *db_file, size_t count, const char **names, dbmode_t open_mode)
{
    dsh_t *dsh;
    dbh_t *handle;

    size_t i;
    int flags;
    DEPOT *dbp;

    if (open_mode == DB_WRITE)
	flags = DP_OWRITER | DP_OCREAT;
    else
	flags = DP_OREADER;

    handle = dbh_init(db_file, count, names);

    if (handle == NULL) return NULL;

    dsh = dsh_init(handle, handle->count, false);

    for (i = 0; i < handle->count; i += 1) {

      dbp = handle->dbp[i] = dpopen(handle->name[i], flags, DB_INITBNUM);

      if (dbp == NULL)
	  goto open_err;

      if (flags & DP_OWRITER) {
	  if (!dpsetalign(dbp, DB_ALIGNSIZE)){
	      dpclose(dbp);
	      goto open_err;
	  }
      }

      if (DEBUG_DATABASE(1)) {
	  dbh_print_names(dsh->dbh, "(qdbm) dpopen( ");
	  fprintf(dbgout, ", %d )\n", open_mode);
      }
      
    }

    return dsh;

 open_err:
    print_error(__FILE__, __LINE__, "(qdbm) dpopen(%s, %d) failed: %s",
		handle->name[i], flags, dperrmsg(dpecode));
    dbh_free(handle);

    return NULL;
}


int db_delete(dsh_t *dsh, const dbv_t *token)
{
    int ret = -1; /* cater for handle->count <= 0 */
    size_t i;
    dbh_t *handle = dsh->dbh;
    DEPOT *dbp;

    for (i = 0; i < handle->count; i += 1) {
      dbp = handle->dbp[i];
      ret = dpout(dbp, token->data, token->leng);

      if (ret == 0) {
	  print_error(__FILE__, __LINE__, "(qdbm) dpout('%.*s'), err: %s",
		  CLAMP_INT_MAX(token->leng),
		      (char *)token->data, dperrmsg(dpecode));
	  exit(EX_ERROR);
      }
      ret = ret ^ 1;	/* ok is 1 in qdbm and 0 in bogofilter */
    }

    return ret;		/* 0 if ok */
}


int db_get_dbvalue(dsh_t *dsh, const dbv_t *token, /*@out@*/ dbv_t *val)
{
    char *data;
    int dsiz;

    dbh_t *handle = dsh->dbh;
    DEPOT *dbp = handle->dbp[dsh->index];

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
    DEPOT *dbp = handle->dbp[dsh->index];

    ret = dpput(dbp, token->data, token->leng, val->data, val->leng, DP_DOVER);

    if (ret == 0) {
	print_error(__FILE__, __LINE__,
		    "(qdbm) db_set_dbvalue( '%.*s' ), err: %d",
		    CLAMP_INT_MAX(token->leng), (char *)token->data, dpecode);
	exit(EX_ERROR);
    }

    db_optimize(dbp, handle->name[dsh->index]);

    return 0;
}


/*
   Close files and clean up.
*/
void db_close(void *vhandle, bool nosync)
{
    dbh_t *handle = vhandle;
    size_t i;

    if (handle == NULL) return;

    if (DEBUG_DATABASE(1)) {
	dbh_print_names(vhandle, "(qdbm) db_close");
	fprintf(dbgout, " %s\n", nosync ? "nosync" : "sync");
    }

    for (i = 0; i < handle->count; i += 1) {
	DEPOT *dbp = handle->dbp[i];

	db_optimize(dbp, handle->name[i]);

	if (!dpclose(dbp))
	    print_error(__FILE__, __LINE__, "(qdbm) dpclose for %s failed: %s",
			handle->name[i], dperrmsg(dpecode));
	handle->dbp[i] = NULL;
    }

    dbh_free(handle);
}


/*
   Flush any data in memory to disk
*/
void db_flush(dsh_t *dsh)
{
    dbh_t *handle = dsh->dbh;
    size_t i;

    for (i = 0; i < handle->count; i++) {
	DEPOT * dbp = handle->dbp[i];

	if (!dpsync(dbp))
	    print_error(__FILE__, __LINE__, "(qdbm) dpsync failed: %s",
			dperrmsg(dpecode));
    }
}


int db_foreach(dsh_t *dsh, db_foreach_t hook, void *userdata)
{
    dbh_t *handle = dsh->dbh;
    DEPOT *dbp = handle->dbp[dsh->index];
    dbv_t dbv_key, dbv_data;
    int ksiz, dsiz;
    char *key, *data;

    int ret = 0;

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
