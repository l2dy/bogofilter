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
#include <time.h>
#include <unistd.h>
#include <db.h>
#include <errno.h>

#include <config.h>
#include "common.h"

#include "datastore.h"
#include "datastore_db.h"
#include "error.h"
#include "maint.h"
#include "xmalloc.h"
#include "xstrdup.h"

#define DBT_init(dbt) do { memset(&dbt, 0, sizeof(DBT)); } while(0)

#if DB_VERSION_MAJOR <= 3 || (DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR == 0)
#define	DB_OPEN(db, file, database, dbtype, flags, mode) db->open(db, file, database, dbtype, flags, mode)
#else
#define	DB_OPEN(db, file, database, dbtype, flags, mode) db->open(db, NULL /*txnid*/, file, database, dbtype, flags, mode)
#endif

/* Function prototypes */

static long db_get_dbvalue(void *vhandle, const char *word, dbv_t *val);
static void db_set_dbvalue(void *vhandle, const char *word, dbv_t *val);

/* Function definitions */

static void db_enforce_locking(dbh_t *handle, const char *func_name){
    if (handle->locked == false){
	print_error(__FILE__, __LINE__, "%s (%s): Attempt to access unlocked handle.", func_name, handle->name);
	exit(2);
    }
}

/* stolen from glibc's byteswap.c */
static long swap_long(long x){
  return ((((x) & 0xff000000) >> 24) | (((x) & 0x00ff0000) >>  8) |
          (((x) & 0x0000ff00) <<  8) | (((x) & 0x000000ff) << 24));
}

static dbh_t *dbh_init(const char *filename, const char *name){
  dbh_t *handle;

  handle = xmalloc(sizeof(dbh_t));

  handle->filename  = xstrdup(filename);
  handle->name	    = xstrdup(name);
  handle->pid	    = getpid();
  handle->locked    = false;
  handle->is_swapped= 0;

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
  Returns: pointer to database handle on success, NULL otherwise.
*/
void *db_open(const char *db_file, const char *name, dbmode_t open_mode){
    int ret;
    dbh_t *handle;
    u_int32_t opt_flags = 0;

    if (open_mode == DB_READ)
      opt_flags = DB_RDONLY;
    else
      opt_flags = DB_CREATE; /*Read-write mode implied. Allow database to be created if necesary */

    handle = dbh_init(db_file, name);
    if ((ret = db_create (&(handle->dbp), NULL, 0)) != 0){
	print_error(__FILE__, __LINE__, "(db) create, err: %d, %s", ret, db_strerror(ret));
    }
    else if ((ret = DB_OPEN(handle->dbp, db_file, NULL, DB_BTREE, opt_flags, 0664)) != 0)
    {
	print_error(__FILE__, __LINE__, "(db) open( %s ), err: %d, %s", db_file, ret, db_strerror(ret));
    }
    else {
      /* see if the database byte order differs from that of the cpu's */
      int had_err = 0;

#if DB_VERSION_MAJOR > 3 || (DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR >= 3)
      if ( (ret = handle->dbp->get_byteswapped (handle->dbp, &(handle->is_swapped))) != 0){
	handle->dbp->err (handle->dbp, ret, "%s (db) get_byteswapped: %s", progname, db_file);
        had_err = 1;
      }
#else
      handle->is_swapped = handle->dbp->get_byteswapped (handle->dbp);
#endif
      if (!had_err)
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
  dbv_t val;
  int  ret;
  long value = 0;
  dbh_t *handle = vhandle;

  ret = db_get_dbvalue(vhandle, word, &val);

  if (ret == 0) {
    value = val.count;
      
    if (DEBUG_DATABASE(2)) {
      fprintf(stderr, "[%lu] db_getvalue (%s): [%s] has value %ld\n",
	      (unsigned long) handle->pid, handle->name, word, value);
    }
    return(value);
  }
  else
    return 0;
}


long db_get_dbvalue(void *vhandle, const char *word, dbv_t *val){
  int  ret;
  DBT db_key;
  DBT db_data;
  char *t;
  long cv[2];

  dbh_t *handle = vhandle;
	
  db_enforce_locking(handle, "db_getvalue");
    
  DBT_init(db_key);
  DBT_init(db_data);

  db_key.data = t = xstrdup(word);
  db_key.size = strlen(word);

  db_data.data = &cv;
  db_data.size = sizeof(cv);
  db_data.ulen = sizeof(cv);

  ret = handle->dbp->get(handle->dbp, NULL, &db_key, &db_data, 0);
  xfree(t);
  switch (ret) {
  case 0:
    memcpy(cv, db_data.data, db_data.ulen);	/* save array from wordlist */
    if (!handle->is_swapped){			/* convert from struct to array */
      val->count = cv[0];
      val->date  = cv[1];
    } else {
      val->count = swap_long(cv[0]);
      val->date  = swap_long(cv[1]);
    }
    if (handle->is_swapped){
      val->count = swap_long(val->count);
      if (db_data.ulen > sizeof(long))
	val->date  = swap_long(val->date);
    }
    return ret;
  case DB_NOTFOUND:
    if (DEBUG_DATABASE(2)) {
      fprintf(stderr, "[%lu] db_getvalue (%s): [%s] not found\n", (unsigned long) handle->pid, handle->name, word);
    }
    return ret;
  default:
    print_error(__FILE__, __LINE__, "(db) db_getvalue( '%s' ), err: %d, %s", word, ret, db_strerror(ret));
    exit(2);
  }
}


/*
Store VALUE in database, using WORD as database key
Notes: Calls exit if an error occurs.
*/
void db_setvalue(void *vhandle, const char * word, long value){
  dbv_t val;
  val.count = value;
  val.date  = today;		/* date in form YYYYMMDD */
  db_set_dbvalue(vhandle, word, &val);
}

void db_setvalue_and_date(void *vhandle, const char * word, long value, long date){
  dbv_t val;
  val.count = value;
  val.date  = date;		/* date in form YYYYMMDD */
  db_set_dbvalue(vhandle, word, &val);
}


void db_set_dbvalue(void *vhandle, const char * word, dbv_t *val){
  int ret;
  DBT db_key;
  DBT db_data;
  long cv[2];
  dbh_t *handle = vhandle;
  char *t;

  db_enforce_locking(handle, "db_set_dbvalue");

  DBT_init(db_key);
  DBT_init(db_data);

  db_key.data = t = xstrdup(word);
  db_key.size = strlen(word);

  if (!handle->is_swapped){		/* convert from struct to array */
    cv[0] = val->count;
    cv[1] = val->date;
  } else {
    val->count = swap_long(cv[0]);
    val->date  = swap_long(cv[1]);
  }

  db_data.data = &cv;			/* and save array in wordlist */
  if (!datestamp_tokens || val->date == 0)
      db_data.size = db_data.ulen = sizeof(cv[0]);
  else
      db_data.size = db_data.ulen = sizeof(cv);

  ret = handle->dbp->put(handle->dbp, NULL, &db_key, &db_data, 0);
  xfree(t);
  if (ret == 0){
    if (DEBUG_DATABASE(2)) {
      fprintf(stderr, "db_set_dbvalue (%s): [%s] has value %ld\n", handle->name, word, val->count);
    }
  }
  else {
    print_error(__FILE__, __LINE__, "(db) db_set_dbvalue( '%s' ), err: %d, %s", word, ret, db_strerror(ret));
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


void db_increment_with_date(void *vhandle, const char *word, long value, long date){
  value = db_getvalue(vhandle, word) + value;
  db_setvalue_and_date(vhandle, word, value < 0 ? 0 : value, date);
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
    print_error(__FILE__, __LINE__, "(db) db_lock: %d, err: %d, %s", cmd, ret, db_strerror(ret));
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

  errno = 0;
  
  if (db_lock(handle, F_SETLKW, F_RDLCK) != 0){
    if (errno == EAGAIN){
  	if (DEBUG_DATABASE(2))
    	  fprintf(stderr, "[%lu] Faked read lock on %s.\n", (unsigned long) handle->pid, handle->filename);
    }
    else {
	print_error(__FILE__, __LINE__, "Error acquiring read lock on %s\n", handle->filename);
	exit(2);
    }
  }

  if (DEBUG_DATABASE(1))
    fprintf(stderr, "[%lu] Got read lock  on %s\n", (unsigned long) handle->pid, handle->filename);

  handle->locked = true;
}

/*
Acquires blocking write lock on database.
*/
void db_lock_writer(void *vhandle){
  dbh_t *handle = vhandle;

  if (DEBUG_DATABASE(1))
    fprintf(stderr, "[%lu] Acquiring write lock on %s\n", (unsigned long) handle->pid, handle->filename);

  if (db_lock(handle, F_SETLKW, F_WRLCK) != 0){
      print_error(__FILE__, __LINE__, "Error acquiring write lock on %s\n", handle->filename);
      exit(2);
  }

  if (DEBUG_DATABASE(1))
    fprintf(stderr, "[%lu] Got write lock on %s\n", (unsigned long) handle->pid, handle->filename);

  handle->locked = true;
}

/*
Releases acquired lock
*/
void db_lock_release(void *vhandle){
  dbh_t *handle = vhandle;

  if (handle->locked){
    if (DEBUG_DATABASE(1))
      fprintf(stderr, "[%lu] Releasing lock on %s\n", (unsigned long) handle->pid, handle->filename);

    if (db_lock(handle, F_SETLK, F_UNLCK) != 0){
	print_error(__FILE__, __LINE__, "Error releasing on %s\n", handle->filename);
	exit(2);
    }
  }
  else if (DEBUG_DATABASE(1)) {
    fprintf(stderr, "[%lu] Attempt to release open lock on %s\n", (unsigned long) handle->pid, handle->filename);
  }

  handle->locked = false;
}

/*
Releases acquired locks on multiple databases
*/
void db_lock_release_list(wordlist_t *list){
  while (list != NULL) {
    if (list->active && ((dbh_t *)list->dbh)->locked)
      db_lock_release(list->dbh);

    list = list->next;
  }
}

static void lock_msg(const dbh_t *handle, int idx, const char *msg, int cmd, int type)
{
  const char *block_type[] = { "nonblocking", "blocking" };
  const char *lock_type[]  = { "write", "read" };

  print_error(__FILE__, __LINE__, "[%lu] [%d] %s %s %s lock on %s", 
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
      
      if (tmp->active == false)
	continue;

      if (DEBUG_DATABASE(1))
	do_lock_msg("Trying");
      
       if (db_lock(handle, cmd, type) == 0){
        if (DEBUG_DATABASE(1))
          do_lock_msg("Got");
        
        handle->locked = true;
       }
       else if (type == F_RDLCK && errno == EAGAIN){
        if (verbose)
          do_lock_msg("Faked");
	
        handle->locked = true;
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
    usleep( 1 + (unsigned long) (999999.0*rand()/(RAND_MAX+1.0)));
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
