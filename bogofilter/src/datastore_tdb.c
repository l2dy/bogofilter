/* $Id$ */

/*****************************************************************************

NAME:
datastore_tdb.c -- implements the datastore, using  tdb.

AUTHORS:
Gyepi Sam <gyepi@praxis-sw.com>   2003

******************************************************************************/

#include "system.h"
#include <tdb.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#include "common.h"
#include "datastore.h"
#include "maint.h"
#include "error.h"
#include "word.h"
#include "xmalloc.h"
#include "xstrdup.h"


typedef struct {
  char *filename;
  size_t count;    
  char *name[2];
  pid_t pid;
  bool locked;
  TDB_CONTEXT  *dbp[2];
} dbh_t;


/* This must come after dbh_t is defined ! */
#include "db_handle_props.h"

static dbh_t *dbh_init(const char *path, size_t count, const char **names)
{
  size_t c;
  dbh_t *handle;

  handle = xmalloc(sizeof(dbh_t));
  memset(handle, 0, sizeof(dbh_t));	/* valgrind */

  handle->count = count;
  handle->filename  = xstrdup(path);
  for (c = 0; c < count ; c += 1) {
	size_t len = strlen(path) + strlen(names[c]) + 2;
	handle->name[c] = xmalloc(len);
	build_path(handle->name[c], len, path, names[c]);
    }
  handle->pid	    = getpid();
  handle->locked    = false;

  return handle;
}

static void dbh_free(/*@only@*/ dbh_t *handle)
{
    if (handle != NULL) {
	size_t c;
	for (c = 0; c < handle->count ; c += 1)
	    xfree(handle->name[c]);
	xfree(handle->filename);
	xfree(handle);
    }
    return;
}


void dbh_print_names(void *vhandle, const char *msg)
{
    dbh_t *handle = vhandle;

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
  dbh_t *handle;
 
  size_t i;
  int tdb_flags = 0;
  int open_flags;

  if (open_mode ==  DB_WRITE) {
    /* tdb_flags = TDB_NOMMAP; */
    open_flags = O_RDWR | O_CREAT;
  }
  else {
    tdb_flags = TDB_NOLOCK;
    open_flags = O_RDONLY;
  }

  handle = dbh_init(db_file, count, names);

  if (handle == NULL)
      return NULL;

  for (i = 0; i < handle->count; i += 1) {
      TDB_CONTEXT *dbp;

      dbp = handle->dbp[i] = tdb_open(handle->name[i], 0, tdb_flags, open_flags, 0664);

      if (dbp == NULL) {
	  print_error(__FILE__, __LINE__, "(db) tdb_open( %s ) failed with error %s", 
		      handle->filename, strerror(errno));
	  exit(2);
	  dbh_free(handle);
	  handle = NULL;
      }
      else {
	  if (DEBUG_DATABASE(1)) {
/*	      fprintf(dbgout, "(db) tdb_open( %s, %s, %d )\n", ... ); */
	      dbh_print_names(handle, "(db) tdb_open( ");
	      fprintf(dbgout, ", %d )\n", open_mode);
	  }
    
	  if (open_mode == DB_WRITE) {
	      if (tdb_lockall(dbp) == 0) {
		  handle->locked = 1;
	      }
	      else {
		  print_error(__FILE__, __LINE__, "(db) tdb_lockall on file (%s) failed with error %s.", 
			      handle->filename, tdb_errorstr(dbp));
		  dbh_free(handle);
		  handle = NULL;
	      }
	  }
      }
  }
  return handle;
}

void db_delete(void *vhandle, const word_t *word)
{
    int ret;
    size_t i;
    dbh_t *handle = vhandle;
    TDB_DATA db_key;

    db_key.dptr = word->text;
    db_key.dsize = word->leng;

    for (i = 0; i < handle->count; i += 1) {
	TDB_CONTEXT *dbp = handle->dbp[i];
	ret = tdb_delete(dbp, db_key);

	if (ret != 0) {
	print_error(__FILE__, __LINE__, "(db) tdb_delete('%s'), err: %d, %s", 
		    word->text, ret, tdb_errorstr(dbp));
	exit(2);
	}
    }

    return;
}

int db_get_dbvalue(void *vhandle, const word_t *word, /*@out@*/ dbv_t *val)
{
  int ret = -1;
  bool found = false;
  size_t i;
  TDB_DATA db_key;
  TDB_DATA db_val;
  uint32_t cv[3] = { 0l, 0l, 0l };

  dbh_t *handle = vhandle;

  db_key.dptr = word->text;
  db_key.dsize = word->leng;

  for (i = 0; i < handle->count; i += 1) {
      TDB_CONTEXT *dbp = handle->dbp[i];
      db_val = tdb_fetch(dbp, db_key);

      if (db_val.dptr != NULL) {
	  if (sizeof(cv) < db_val.dsize) {
	      print_error(__FILE__, __LINE__, "(db) db_get_dbvalue( '%s' ), size error %d::%d", 
			  word->text, sizeof(cv), db_val.dsize);
	      exit(2);
	  }

	  memcpy(cv, db_val.dptr, db_val.dsize);

	  found = true;
	  memcpy(cv, db_val.dptr, db_val.dsize);
#if	0	/* Gyepi - TDB */
	  val->count = cv[0];
	  val->date  = cv[1];

#else
	  if (handle->count == 1) {
	      val->spamcount = cv[0];
	      val->goodcount = cv[1];
	      val->date      = cv[2];
	  } else {
	      uint32_t date;
	      val->count[i] = cv[0];
	      date          = cv[1];
	      val->date = max(val->date, date);
	  }
#endif
	  free(db_val.dptr);
	  ret = 0;
      }
  }

  if (!found && DEBUG_DATABASE(3)) {
      fprintf(dbgout, "db_getvalues (%s): [",
	      handle->name[0]);
      word_puts(word, 0, dbgout);
      fprintf(dbgout, "] has values %lu,%lu\n",
	      (unsigned long)val->spamcount,
	      (unsigned long)val->goodcount);
  }

  return found ? 0 : ret;
}


void db_set_dbvalue(void *vhandle, const word_t *word, dbv_t *val)
{
  int ret;
  TDB_DATA db_key;
  TDB_DATA db_data;
  size_t i;
  uint32_t cv[3];
  dbh_t *handle = vhandle;

  db_key.dptr = word->text;
  db_key.dsize = word->leng;

#if	0	/* Gyepi - TDB */
  cv[0] = val->count;
  cv[1] = val->date;

  db_data.dptr = (char *)&cv;			/* and save array in wordlist */

  if (!datestamp_tokens || val->date == 0)
      db_data.dsize = sizeof(cv[0]);
  else
      db_data.dsize = sizeof(cv);

  ret = tdb_store(handle->dbp, db_key, db_data, TDB_REPLACE);

  if (ret == 0) {
    if (DEBUG_DATABASE(3)) {
      fprintf(dbgout, "db_set_dbvalue (%s): [",
	      handle->name);
      word_puts(word, 0, dbgout);
      fprintf(dbgout, "] has value %lu\n",
	      (unsigned long)val->count);
    }
  }
  else {
    print_error(__FILE__, __LINE__, "(db) db_set_dbvalue( '%s' ), err: %d, %s", word->text, ret, tdb_errorstr());
    exit(2);
  }
#else
    for (i = 0; i < handle->count; i += 1) {
	TDB_CONTEXT *dbp = handle->dbp[i];
	if (handle->count == 1) {
	    cv[0] = val->spamcount;
	    cv[1] = val->goodcount;
	    cv[2] = val->date;
	} else {
	    cv[0] = val->count[i];
	    cv[1] = val->date;
	}
	db_data.dptr = (char *) &cv;		/* and save array in wordlist */
	if (!datestamp_tokens || val->date == 0)
	    db_data.dsize = 2 * sizeof(cv[0]);
	else
	    db_data.dsize = sizeof(cv);

	ret = tdb_store(dbp, db_key, db_data, TDB_REPLACE);

	if (ret == 0) {
	    if (DEBUG_DATABASE(3)) {
		dbh_print_names(handle, "db_set_dbvalue");
		fputs(": [", dbgout);
		word_puts(word, 0, dbgout);
		fprintf(dbgout, "] has values %lu,%lu\n",
			(unsigned long)val->spamcount,
			(unsigned long)val->goodcount);
	    }
	}
	else {
	    print_error(__FILE__, __LINE__, "(db) db_set_dbvalue( '%s' ), err: %d, %s", word->text, ret, tdb_errorstr(dbp));
	    exit(2);
	}
    }
#endif
}


/* Close files and clean up. */
void db_close(void *vhandle, bool nosync)
{
  dbh_t *handle = vhandle;
  int ret;
  size_t i;

  if (DEBUG_DATABASE(1)) {
      dbh_print_names(vhandle, "db_close");
      fprintf(dbgout, " %s\n", nosync ? "nosync" : "sync");
  }

  if (handle == NULL) return;
  
  if (handle->locked) {
      for (i = 0; i < handle->count; i += 1) {
	  TDB_CONTEXT *dbp = handle->dbp[i];
	  tdb_unlockall(dbp);
      }
  }

  for (i = 0; i < handle->count; i += 1) {
      TDB_CONTEXT *dbp = handle->dbp[i];
	
      if ((ret = tdb_close(dbp))) {
	  print_error(__FILE__, __LINE__, "(db) tdb_close on file %s failed with error %s", handle->filename,  tdb_errorstr(dbp));
      }
  }

  dbh_free(handle);
}

/*
 flush any data in memory to disk
*/
void db_flush( /*@unused@*/ __attribute__ ((unused)) void *vhandle)
{
  /* noop */
}

typedef struct userdata_t {
  db_foreach_t hook;
  void *userdata;
} userdata_t;


static int tdb_traversor(/*@unused@*/ __attribute__ ((unused)) TDB_CONTEXT *tdb_handle, TDB_DATA key, TDB_DATA data, void *userdata)
{
  uint32_t cv[3];
  word_t w_key, w_data;
  userdata_t *hookdata = userdata;

  memcpy(&cv, data.dptr, data.dsize);

  /* switch to "word_t *" variables */
  w_key.text = key.dptr;
  w_key.leng = key.dsize;
  w_data.text = data.dptr;
  w_data.leng = data.dsize;

  /* call user function */
  return hookdata->hook(&w_key, &w_data, hookdata->userdata);
}


int db_foreach(void *vhandle, db_foreach_t hook, void *userdata)
{
  dbh_t *handle = vhandle;
  int ret = 0;
  size_t i;
  userdata_t hookdata;
  
  hookdata.hook = hook;
  hookdata.userdata = userdata;

  for (i = 0; i < handle->count; i += 1) {
      TDB_CONTEXT *dbp = handle->dbp[i];

      ret = tdb_traverse(dbp, tdb_traversor, (void *)&hookdata);
      if (ret == -1) {
	  print_error(__FILE__, __LINE__, "(db) db_foreach err: %d, %s", ret, tdb_errorstr(dbp));
      }
      else {
	  /* tdb_traverse returns a count of records. We just want success or failure */ 
	  ret = 0;
      }
  }

  return ret;
}
