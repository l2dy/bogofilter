/* $Id$ */

/*****************************************************************************

NAME:
   datastore_db.c -- implements the datastore, using Berkeley DB 

AUTHOR:
   Gyepi Sam <gyepi@praxis-sw.com>

******************************************************************************/

#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <db.h>
#include <errno.h>

#include "config.h"
#ifdef	HAVE_SYSLOG_H
#include <syslog.h>
#endif

#include "xmalloc.h"
#include "xstrdup.h"
#include "datastore.h"
#include "datastore_db.h"
#include "globals.h"

#define DBT_init(dbt) do { memset(&dbt, 0, sizeof(DBT)); } while(0)

#ifndef	HAVE_SYSLOG_H
#define SYSLOG_ERROR(format, filename)
#else
#define SYSLOG_ERROR(format, filename)      if (logflag) syslog( LOG_ERR, format, filename)
#endif

static void db_enforce_locking(dbh_t *handle, const char *func_name){
  if (handle->locked == FALSE){
    fprintf(stderr, "%s (%s): Attempt to access unlocked handle.\n", func_name, handle->name);
    exit(2);
  }
}

static dbh_t *dbh_init(const char *filename, const char *name){
  dbh_t *handle;

  handle = xmalloc(sizeof(dbh_t));

  handle->filename  = xstrdup(filename);
  handle->name	    = xstrdup(name);
  handle->pid	    = getpid();
  handle->locked    = FALSE;

  return handle;
}

static void dbh_free(dbh_t *handle){
	if (handle == NULL) return;
	if (handle->filename != NULL) xfree(handle->filename);
	if (handle->name != NULL) xfree(handle->name);
        xfree(handle);
}

/*
  Initialize database.
  Returns: pointer database handle on success, NULL otherwise.
*/
void *db_open(const char *db_file, const char *name, dbmode_t open_mode){
    int ret;
    dbh_t *handle;
    u_int32_t opt_flags = 0;

    if (open_mode == DB_READ)
      opt_flags = DB_RDONLY;
    else
      opt_flags =  DB_CREATE; /*Read-write mode implied. Allow database to be created if necesary */

    handle = dbh_init(db_file, name);
    if ((ret = db_create (&(handle->dbp), NULL, 0)) != 0){
	   fprintf (stderr, "%s (db) db_create: %s\n", progname, db_strerror (ret));
    }
    else if
#if DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR > 0
    ((ret = handle->dbp->open(handle->dbp, NULL, db_file, NULL, DB_BTREE, opt_flags, 0664)) != 0)
#else
    ((ret = handle->dbp->open (handle->dbp, db_file, NULL, DB_BTREE, opt_flags, 0664)) != 0)
#endif	    
    {
	handle->dbp->err (handle->dbp, ret, "%s (db) open: %s", progname, db_file);
    }
    else {
      return (void *)handle;
    }

    dbh_free(handle);
    return NULL;
}

/*  
    Retrieve numeric value associated with word.
    Returns: value if the the word is found in database, 0 if the word is not found.
    Notes: Will call exit if an error occurs.
*/
long db_getvalue(void *vhandle, const char *word){
    DBT db_key;
    DBT db_data;
    long value;
    int  ret;
    char *t;

    dbh_t *handle = vhandle;
	
    db_enforce_locking(handle, "db_getvalue");
    
    DBT_init(db_key);
    DBT_init(db_data);

    db_key.data = t = xstrdup(word);
    db_key.size = strlen(word);

    ret = handle->dbp->get(handle->dbp, NULL, &db_key, &db_data, 0);
    xfree(t);
    if (ret == 0) {
      value = *(long *)db_data.data;

      if (DEBUG_DATABASE(2)) {
        fprintf(stderr, "[%lu] db_getvalue (%s): [%s] has value %ld\n",
		(unsigned long) handle->pid, handle->name, word, value);
      }

      return(value);
    }
    else if (ret == DB_NOTFOUND){
      if (DEBUG_DATABASE(2)) {
        fprintf(stderr, "[%lu] db_getvalue (%s): [%s] not found\n", (unsigned long) handle->pid, handle->name, word);
      }
      return 0;
    }
    else {
	handle->dbp->err (handle->dbp, ret, "%s (db) db_getvalue: %s", progname, word);
	exit(2);
    }
}


/*
Store VALUE in database, using WORD as database key
Notes: Calls exit if an error occurs.
*/
void db_setvalue(void *vhandle, const char * word, long value){
    int ret;
    DBT key;
    DBT data;
    dbh_t *handle = vhandle;
    char *t;
   
    db_enforce_locking(handle, "db_setvalue");

    DBT_init(key);
    DBT_init(data);

    key.data = t = xstrdup(word);
    key.size = strlen(word);

    data.data = &value;
    data.size = sizeof(long);

    ret = handle->dbp->put(handle->dbp, NULL, &key, &data, 0);
    xfree(t);
    if (ret == 0){
      if (DEBUG_DATABASE(2)) {
	fprintf(stderr, "db_setvalue (%s): [%s] has value %ld\n", handle->name, word, value);
      }
    }
    else 
    {
	handle->dbp->err (handle->dbp, ret, "%s (db) db_setvalue: %s", progname, word);
	exit(2);
    }
}


/*
  Increment count associated with WORD, by VALUE.
 */
void db_increment(void *vhandle, const char *word, long value){
  value = db_getvalue(vhandle, word) + value;
  db_setvalue(vhandle, word, value < 0 ? 0 : value);
}


/*
  Decrement count associated with WORD by VALUE,
  if WORD exists in the database.
*/
void db_decrement(void *vhandle, const char *word, long value){
  value = db_getvalue(vhandle, word) - value;
  db_setvalue(vhandle, word, value < 0 ? 0 : value);
}

/*
  Get the number of messages associated with database.
*/
long db_getcount(void *vhandle){
  return db_getvalue(vhandle, MSG_COUNT_TOK);
}

/*
 Set the number of messages associated with database.
*/
void db_setcount(void *vhandle, long count){
  db_setvalue(vhandle, MSG_COUNT_TOK, count);
}


/* Close files and clean up. */
void db_close(void *vhandle){  
  dbh_t *handle = vhandle;
  if (handle == NULL) return;
  handle->dbp->close(handle->dbp, 0);
  dbh_free(handle);
}

/*
 flush any data in memory to disk
*/
void db_flush(void *vhandle){
  dbh_t *handle = vhandle;
  handle->dbp->sync(handle->dbp, 0);
}


/* implements locking. */
static int db_lock(dbh_t *handle, int cmd, int type){
  int fd;
  int ret;
  struct flock lock;

  if ( (ret = handle->dbp->fd(handle->dbp, &fd)) != 0){
    handle->dbp->err (handle->dbp, ret, "%s (db) db_lock:", progname);
    exit(2);
  }

  lock.l_type = type;
  lock.l_start = 0;
  lock.l_whence = SEEK_END;
  lock.l_len = 0;
  return (fcntl(fd, cmd, &lock));
}

/*
Acquires blocking read lock on database.
*/
void db_lock_reader(void *vhandle){
  dbh_t *handle = vhandle;

  if (DEBUG_DATABASE(1))
    fprintf(stderr, "[%lu] Acquiring read lock  on %s\n", (unsigned long) handle->pid, handle->filename);

  if (db_lock(handle, F_SETLKW, F_RDLCK) != 0){
    fprintf(stderr, "[%lu] Error acquiring read lock on %s\n", (unsigned long) handle->pid, handle->filename);
    SYSLOG_ERROR( "Error acquiring read lock on %s\n", handle->filename);
    exit(2);
  }

  if (DEBUG_DATABASE(1))
    fprintf(stderr, "[%lu] Got read lock  on %s\n", (unsigned long) handle->pid, handle->filename);

  handle->locked = TRUE;
}

/*
Acquires blocking write lock on database.
*/
void db_lock_writer(void *vhandle){
  dbh_t *handle = vhandle;

  if (DEBUG_DATABASE(1))
    fprintf(stderr, "[%lu] Acquiring write lock on %s\n", (unsigned long) handle->pid, handle->filename);

  if (db_lock(handle, F_SETLKW, F_WRLCK) != 0){
    fprintf(stderr, "[%lu] Error acquiring write lock on %s\n", (unsigned long) handle->pid, handle->filename);
    SYSLOG_ERROR( "Error acquiring write lock on %s\n", handle->filename);
    exit(2);
  }

  if (DEBUG_DATABASE(1))
    fprintf(stderr, "[%lu] Got write lock on %s\n", (unsigned long) handle->pid, handle->filename);

  handle->locked = TRUE;
}

/*
Releases acquired lock
*/
void db_lock_release(void *vhandle){
  dbh_t *handle = vhandle;

  if (handle->locked == TRUE){
    if (DEBUG_DATABASE(1))
      fprintf(stderr, "[%lu] Releasing lock on %s\n", (unsigned long) handle->pid, handle->filename);

    if (db_lock(handle, F_SETLK, F_UNLCK) != 0){
      fprintf(stderr, "[%lu] Error releasing on %s\n", (unsigned long) handle->pid, handle->filename);
      SYSLOG_ERROR( "Error releasing on %s\n", handle->filename);
      exit(2);
    }
  }
  else if (DEBUG_DATABASE(1)) {
    fprintf(stderr, "[%lu] Attempt to release open lock on %s\n", (unsigned long) handle->pid, handle->filename);
  }

  handle->locked = FALSE;
}

/*
Releases acquired locks on multiple databases
*/
void db_lock_release_list(wordlist_t *list){
  while (list != NULL) {
    if ((list->active == TRUE) && (((dbh_t *)list->dbh)->locked == TRUE))
      db_lock_release(list->dbh);

    list = list->next;
  }
}

static void lock_msg(const dbh_t *handle, int idx, const char *msg, int cmd, int type)
{

  const char *block_type[] = { "nonblocking", "blocking" };
  const char *lock_type[]  = { "write", "read" };
                                                     
  fprintf(stderr, "[%lu] [%d] %s %s %s lock on %s\n", 
	          (unsigned long)handle->pid,
                  idx,
		  msg,
		  block_type[cmd == F_SETLKW],
		  lock_type[type == F_RDLCK],
		  handle->filename);
}

/*
Acquires locks on multiple database.

Uses the following protocol  to avoid deadlock:

1. The first database in the list is locked in blocking mode,
   all subsequent databases are locked in non-blocking mode.

2. If a lock attempt on a database fails, all locks on existing databases are relinquished,
   and the whole operation restarted. In order to avoid syncronicity among contending lockers,
   each locker sleeps for some small, random time period before restarting.

*/
static void db_lock_list(wordlist_t *list, int type){

#define do_lock_msg(msg) lock_msg(handle, i, msg, cmd, type)
  int i;
  int cmd;
  wordlist_t *tmp;

  for(;;){
    for (tmp = list, i = 0, cmd = F_SETLKW; tmp != NULL; tmp = tmp->next, i++, cmd = F_SETLK){

      dbh_t *handle = tmp->dbh;
      
      if (tmp->active == FALSE)
	continue;

      if (DEBUG_DATABASE(1))
	do_lock_msg("Trying");
      
      if (db_lock(handle, cmd, type) == 0){
        if (DEBUG_DATABASE(1))
          do_lock_msg("Got");
        
        handle->locked = TRUE;
      }
      else if (errno == EACCES || errno == EAGAIN || errno == EINTR){
        if (verbose)
          do_lock_msg("Failed to acquire");

        break;
      }
      else {
        do_lock_msg("Error acquiring");
	exit(2);
      }
    }
    /* All locks acquired. */
    if (tmp == NULL)
      break;

    /* Some locks failed. Release all acquired locks */
    db_lock_release_list(list);

    /*sleep for short, random time between 1 microsecond and 1 second */
    usleep( 1 + (unsigned long) (1000.0*rand()/(RAND_MAX+1.0)));
  }
}


/*
Acquires read locks on multiple databases.
*/
void db_lock_reader_list(wordlist_t *list){
  db_lock_list(list, F_RDLCK);
}

/*
Acquires write locks on multiple database.
*/
void db_lock_writer_list(wordlist_t *list){
  db_lock_list(list, F_WRLCK);
}
