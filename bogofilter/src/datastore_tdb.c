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

#include <unistd.h>
#include <errno.h>

#include "datastore.h"
#include "datastore_tdb.h"
#include "maint.h"
#include "error.h"
#include "xmalloc.h"
#include "xstrdup.h"

typedef struct {
    char *path;
    size_t count;
    char *name[IX_SIZE];
    pid_t pid;
    bool locked;
    TDB_CONTEXT *dbp[IX_SIZE];
} dbh_t;

/* Function definitions */

const char *db_version_str(void)
{
    return "TrivialDB";
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
    handle->pid = getpid();
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


void dbh_print_names(dsh_t *dsh, const char *msg)
{
    dbh_t *handle = dsh->dbh;

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
    int tdb_flags = 0;
    int open_flags;
    TDB_CONTEXT *dbp;

    if (open_mode == DB_WRITE) {
	open_flags = O_RDWR | O_CREAT;
    }
    else {
    	tdb_flags = TDB_NOLOCK;
    	open_flags = O_RDONLY;
    }

    handle = dbh_init(db_file, count, names);

    if (handle == NULL) return NULL;

    dsh = dsh_init(handle, handle->count, false);

    for (i = 0; i < handle->count; i += 1) {

      dbp = handle->dbp[i] = tdb_open(handle->name[i], 0, tdb_flags, open_flags, 0664);

      if (dbp == NULL)
	  goto open_err;

      if (DEBUG_DATABASE(1)) {
	  dbh_print_names(dsh, "(db) tdb_open( ");
	  fprintf(dbgout, ", %d )\n", open_mode);
      }
      
      if (open_mode == DB_WRITE) {
	  if (tdb_lockall(dbp) == 0) {
	      handle->locked = 1;
	  }
	  else {
	      print_error(__FILE__, __LINE__, "(db) tdb_lockall on file (%s) failed with error %s.",
			  handle->name[i], tdb_errorstr(dbp));
	      goto open_err;
	  }
      }
    }

    return dsh;

 open_err:
    dbh_free(handle);

    return NULL;
}

int db_delete(dsh_t *dsh, const dbv_t *token)
{
    int ret;
    size_t i;
    dbh_t *handle = dsh->dbh;
    TDB_DATA db_key;
    TDB_CONTEXT *dbp;

    db_key.dptr = token->data;
    db_key.dsize = token->leng;

    for (i = 0; i < handle->count; i += 1) {
      dbp = handle->dbp[i];
      ret = tdb_delete(dbp, db_key);

      if (ret != 0) {
          print_error(__FILE__, __LINE__, "(db) tdb_delete('%s'), err: %d, %s",
                      (char *)token->data, ret, tdb_errorstr(dbp));
          exit(EX_ERROR);
      }
    }

    return ret;
}

int db_get_dbvalue(dsh_t *dsh, const dbv_t *token, /*@out@*/ dbv_t *val)
{
    TDB_DATA db_key;
    TDB_DATA db_data;

    dbh_t *handle = dsh->dbh;
    TDB_CONTEXT *dbp = handle->dbp[dsh->index];

    db_key.dptr = token->data;
    db_key.dsize = token->size;

    db_data = tdb_fetch(dbp, db_key);

    if (db_data.dptr == NULL)
	return DB_NOTFOUND;

    if (val->size < db_data.dsize) {
	print_error(__FILE__, __LINE__, "(db) db_get_dbvalue( '%s' ), size error %d::%d",
		    (char *)token->data, val->size, db_data.dsize);
	exit(EX_ERROR);
    }

    val->leng = db_data.dsize;		/* read count */
    memcpy(val->data, db_data.dptr, db_data.dsize);
    
    free(db_data.dptr);

    return 0;
}


int db_set_dbvalue(dsh_t *dsh, const dbv_t *token, dbv_t *val)
{
    int ret;
    TDB_DATA db_key;
    TDB_DATA db_data;

    dbh_t *handle = dsh->dbh;
    TDB_CONTEXT *dbp = handle->dbp[dsh->index];

    db_key.dptr = token->data;
    db_key.dsize = token->leng;

    db_data.dptr = val->data;
    db_data.dsize = val->leng;		/* write count */

    ret = tdb_store(dbp, db_key, db_data, TDB_REPLACE);

    if (ret != 0) {
	print_error(__FILE__, __LINE__, "(db) db_set_dbvalue( '%*s' ), err: %d, %s",
		    token->size, (char *)token->data, ret, tdb_errorstr(dbp));
	exit(EX_ERROR);
    }

    return ret;
}


/* Close files and clean up. */
void db_close(void *vhandle, bool nosync)
{
    dbh_t *handle = vhandle;
    int ret;
    size_t i;

    if (handle == NULL) return;

    if (DEBUG_DATABASE(1)) {
      dbh_print_names(vhandle, "db_close");
      fprintf(dbgout, " %s\n", nosync ? "nosync" : "sync");
    }

    if (handle->locked) {
        for (i = 0; i < handle->count; i += 1) {
            tdb_unlockall(handle->dbp[i]);
        }
    }

    for (i = 0; i < handle->count; i += 1) {
        if ((ret = tdb_close(handle->dbp[i]))) {
            print_error(__FILE__, __LINE__, "(db) tdb_close on file %s failed with error %s",
			handle->path, tdb_errorstr(handle->dbp[i]));
        }
    }

    dbh_free(handle);
}

/*
 flush any data in memory to disk
*/
void db_flush(/*@unused@*/  __attribute__ ((unused)) dsh_t *dsh)
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

    /* Question: Is there a way to avoid using malloc/free? */

    /* switch to "dbv_t *" variables */
    dbv_key.leng = key.dsize;
    dbv_key.data = xmalloc(dbv_key.leng+1);
    memcpy(dbv_key.data, key.dptr, dbv_key.leng);
    ((char *)dbv_key.data)[dbv_key.leng] = '\0';

    dbv_data.data = data.dptr;
    dbv_data.leng = data.dsize;		/* read count */
    dbv_data.size = data.dsize;

    /* call user function */
    rc = hookdata->hook(&dbv_key, &dbv_data, hookdata->userdata);

    xfree(dbv_key.data);

    return rc;
}


int db_foreach(dsh_t *dsh, db_foreach_t hook, void *userdata)
{
    dbh_t *handle = dsh->dbh;
    TDB_CONTEXT *dbp = handle->dbp[dsh->index];

    int ret;

    userdata_t hookdata;

    hookdata.hook = hook;
    hookdata.userdata = userdata;

    ret = tdb_traverse(dbp, tdb_traversor, (void *) &hookdata);
    if (ret == -1) {
	print_error(__FILE__, __LINE__, "(db) db_foreach err: %d, %s",
		    ret, tdb_errorstr(dbp));
	exit(EX_ERROR);
    }

    return 0;
}
