/* $Id$ */

/*****************************************************************************

NAME:
datastore_qdbm.c -- implements the datastore, using qdbm.

AUTHORS:
Gyepi Sam <gyepi@praxis-sw.com>    2003
Stefan Bellon <sbellon@sbellon.de> 2003

******************************************************************************/

#include "common.h"

#include <relic.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <unistd.h>
#include <errno.h>

#include "datastore.h"
#include "datastore_db.h"
#include "maint.h"
#include "error.h"
#include "paths.h"
#include "xmalloc.h"
#include "xstrdup.h"

typedef struct {
    char *path;
    size_t count;
    char *name[IX_SIZE];
    pid_t pid;
    bool locked;
    DBM *dbp[IX_SIZE];
} dbh_t;

/* Function definitions */

const char *db_version_str(void)
{
    return "QDBM";
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
    DBM *dbp;

    if (open_mode == DB_WRITE)
	flags = O_RDWR | O_CREAT;
    else
    	flags = O_RDONLY;

    handle = dbh_init(db_file, count, names);

    if (handle == NULL) return NULL;

    dsh = dsh_init(handle, handle->count, false);

    for (i = 0; i < handle->count; i += 1) {

      dbp = handle->dbp[i] = dbm_open(handle->name[i], flags, 0664);

      if (dbp == NULL)
	  goto open_err;

      if (DEBUG_DATABASE(1)) {
	  dbh_print_names(dsh->dbh, "(db) dbm_open( ");
	  fprintf(dbgout, ", %d )\n", open_mode);
      }
      
    }

    return dsh;

 open_err:
    dbh_free(handle);

    return NULL;
}

int db_delete(dsh_t *dsh, const dbv_t *token)
{
    int ret = -1; /* cater for handle->count <= 0 */
    size_t i;
    dbh_t *handle = dsh->dbh;
    datum db_key;
    DBM *dbp;

    db_key.dptr = token->data;
    db_key.dsize = token->leng;

    for (i = 0; i < handle->count; i += 1) {
      dbp = handle->dbp[i];
      ret = dbm_delete(dbp, db_key);

      if (ret != 0) {
          print_error(__FILE__, __LINE__, "(db) dbm_delete('%s'), err: %d",
                      (char *)token->data, ret);
          exit(EX_ERROR);
      }
    }

    return ret;
}

int db_get_dbvalue(dsh_t *dsh, const dbv_t *token, /*@out@*/ dbv_t *val)
{
    datum db_key;
    datum db_data;

    dbh_t *handle = dsh->dbh;
    DBM *dbp = handle->dbp[dsh->index];

    db_key.dptr = token->data;
    db_key.dsize = token->size;

    db_data = dbm_fetch(dbp, db_key);

    if (db_data.dptr == NULL)
	return DS_NOTFOUND;

    if (val->size < db_data.dsize) {
	print_error(__FILE__, __LINE__, "(db) db_get_dbvalue( '%s' ), size error %d::%d",
		    (char *)token->data, val->size, db_data.dsize);
	exit(EX_ERROR);
    }

    val->leng = db_data.dsize;		/* read count */
    memcpy(val->data, db_data.dptr, db_data.dsize);
    
    // don't free db_data.dptr as qdbm/relic has it's own internal buffer!

    return 0;
}


int db_set_dbvalue(dsh_t *dsh, const dbv_t *token, dbv_t *val)
{
    int ret;
    datum db_key;
    datum db_data;

    dbh_t *handle = dsh->dbh;
    DBM *dbp = handle->dbp[dsh->index];

    db_key.dptr = token->data;
    db_key.dsize = token->leng;

    db_data.dptr = val->data;
    db_data.dsize = val->leng;		/* write count */

    ret = dbm_store(dbp, db_key, db_data, DBM_REPLACE);

    if (ret != 0) {
	print_error(__FILE__, __LINE__, "(db) db_set_dbvalue( '%*s' ), err: %d",
		    token->size, (char *)token->data, ret);
	exit(EX_ERROR);
    }

    return ret;
}


/* Close files and clean up. */
void db_close(void *vhandle, bool nosync)
{
    dbh_t *handle = vhandle;
    size_t i;

    if (handle == NULL) return;

    if (DEBUG_DATABASE(1)) {
      dbh_print_names(vhandle, "db_close");
      fprintf(dbgout, " %s\n", nosync ? "nosync" : "sync");
    }

    for (i = 0; i < handle->count; i += 1) {
        dbm_close(handle->dbp[i]);
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

int db_foreach(dsh_t *dsh, db_foreach_t hook, void *userdata)
{
    dbh_t *handle = dsh->dbh;
    DBM *dbp = handle->dbp[dsh->index];
    datum key, data;
    dbv_t dbv_key, dbv_data;

    int ret = 0;

    key = dbm_firstkey(dbp);
    while (key.dptr) {
        data = dbm_fetch(dbp, key);
        if (data.dptr) {
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
           ret = hook(&dbv_key, &dbv_data, userdata);

           xfree(dbv_key.data);

           if (ret != 0)
              break;
        }
        key = dbm_nextkey(dbp);
    }

    if (ret == -1) {
	print_error(__FILE__, __LINE__, "(db) db_foreach err: %d", ret);
	exit(EX_ERROR);
    }

    return 0;
}

const char *db_str_err(int e) {
    return "QDBM error";
}
