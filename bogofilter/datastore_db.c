/* $Id$ */

/*****************************************************************************

NAME:
datastore_db.c -- implements the datastore, using Berkeley DB

AUTHORS:
Gyepi Sam <gyepi@praxis-sw.com>   2002 - 2003
Matthias Andree <matthias.andree@gmx.de> 2003

******************************************************************************/

#include <db.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <db.h>
#include <errno.h>
#include <assert.h>

#include <config.h>
#include "common.h"

#include "datastore.h"
#include "error.h"
#include "maint.h"
#include "swap.h"
#include "xmalloc.h"
#include "xstrdup.h"

#define MSG_COUNT_TOK ".MSG_COUNT"

typedef struct {
  char *filename;
  char *name;
  int fd;
  dbmode_t open_mode;
  DB *dbp;
  pid_t pid;
  bool locked;
  int  is_swapped;
} dbh_t;

#define DBT_init(dbt) do { memset(&dbt, 0, sizeof(DBT)); } while(0)

#define DB_AT_LEAST(maj, min) ((DB_VERSION_MAJOR > (maj)) || ((DB_VERSION_MAJOR == (maj)) && (DB_VERSION_MINOR >= (min))))

#if DB_AT_LEAST(4,1)
#define	DB_OPEN(db, file, database, dbtype, flags, mode) db->open(db, NULL /*txnid*/, file, database, dbtype, flags, mode)
#else
#define	DB_OPEN(db, file, database, dbtype, flags, mode) db->open(db, file, database, dbtype, flags, mode)
#endif

/* Function prototypes */

static int db_get_dbvalue(void *vhandle, const char *word, dbv_t *val);
static void db_set_dbvalue(void *vhandle, const char *word, dbv_t *val);
static int db_lock(int fd, int cmd, short int type);

/* Function definitions */

static void db_enforce_locking(dbh_t *handle, const char *func_name){
    if (handle->locked == false){
	print_error(__FILE__, __LINE__, "%s (%s): Attempt to access unlocked handle.", func_name, handle->name);
	exit(2);
    }
}

static dbh_t *dbh_init(const char *filename, const char *name) {
  dbh_t *handle;

  handle = xmalloc(sizeof(dbh_t));
  memset(handle, 0, sizeof(dbh_t));	/* valgrind */

  handle->filename  = xstrdup(filename);
  handle->name	    = xstrdup(name);
  handle->pid	    = getpid();
  handle->locked    = false;
  handle->is_swapped= 0;
  handle->fd	    = -1; /* for lock */

  return handle;
}

static void dbh_free(/*@only@*/ dbh_t *handle){
    if (handle == NULL) return;
    xfree(handle->filename);
    xfree(handle->name);
    xfree(handle);
}

/*
  Initialize database.
  Returns: pointer to database handle on success, NULL otherwise.
*/
void *db_open(const char *db_file, const char *name, dbmode_t open_mode,
	const char *dir)
{
    int ret;
    dbh_t *handle;
    uint32_t opt_flags = 0;

    assert(dir && *dir);

    if (open_mode == DB_READ)
	opt_flags = DB_RDONLY;
    else
	/* Read-write mode implied.  Allow database to be created if
	 * necessary. DB_EXCL makes sure out locking doesn't fail if two
	 * applications try to create a DB at the same time. */
	opt_flags = 0;

    handle = dbh_init(db_file, name);
    handle->open_mode = open_mode;
    /* create DB handle */
    if ((ret = db_create (&(handle->dbp), NULL, 0)) != 0) {
	print_error(__FILE__, __LINE__, "(db) create, err: %d, %s",
		ret, db_strerror(ret));
	goto open_err;
    }

    /* open data base */
    if ((ret = DB_OPEN(handle->dbp, db_file, NULL, DB_BTREE, opt_flags, 0664)) != 0 &&
	    (ret = DB_OPEN(handle->dbp, db_file, NULL, DB_BTREE, opt_flags |  DB_CREATE | DB_EXCL, 0664)) != 0) {
	print_error(__FILE__, __LINE__, "(db) open( %s ), err: %d, %s",
		db_file, ret, db_strerror(ret));
	goto open_err;
    }

    if (DEBUG_DATABASE(1)) {
      fprintf(dbgout, "db_open( %s, %s, %d )\n", name, db_file, open_mode);
    }

    /* see if the database byte order differs from that of the cpu's */
#if DB_AT_LEAST(3,3)
    ret = handle->dbp->get_byteswapped (handle->dbp, &(handle->is_swapped));
#else
    ret = 0;
    handle->is_swapped = handle->dbp->get_byteswapped (handle->dbp);
#endif
    if (ret != 0) {
	handle->dbp->err (handle->dbp, ret, "%s (db) get_byteswapped: %s",
		progname, db_file);
	db_close(handle);
	goto open_err;
    }

    ret = handle->dbp->fd(handle->dbp, &handle->fd);
    if (ret < 0) {
	handle->dbp->err (handle->dbp, ret, "%s (db) fd: %s",
		progname, db_file);
	db_close(handle);
	goto open_err;
    }

    if (db_lock(handle->fd, F_SETLK,
		(short int)(open_mode == DB_READ ? F_RDLCK : F_WRLCK)))
    {
	int e = errno;
	handle->fd = -1;
	db_close(handle);
	errno = e;
	/* do not goto open_err here, db_close frees the handle! */
	return NULL;
    }

    handle->locked = true;
    return (void *)handle;

open_err:
    dbh_free(handle);
    return NULL;
}

/*
    Retrieve numeric value associated with word.
    Returns: value if the the word is found in database,
    0 if the word is not found.
    Notes: Will call exit if an error occurs.
*/
uint32_t db_getvalue(void *vhandle, const char *word){
  dbv_t val;
  uint32_t ret;
  uint32_t value = 0;
  dbh_t *handle = vhandle;

  ret = db_get_dbvalue(vhandle, word, &val);

  if (ret == 0) {
    value = val.count;

    if (DEBUG_DATABASE(2)) {
      fprintf(dbgout, "[%lu] db_getvalue (%s): [%s] has value %lu\n",
	      (unsigned long) handle->pid, handle->name, word, (unsigned long)value);
    }
    return(value);
  }
  else
    return 0;
}

void db_delete(void *vhandle, const char *word) {
    int ret;
    dbh_t *handle = vhandle;
    char *t;

    DBT db_key;
    DBT_init(db_key);

    db_key.data = t = xstrdup(word);
    db_key.size = strlen(word);
    ret = handle->dbp->del(handle->dbp, NULL, &db_key, 0);
    free(t);
    if (ret == 0 || ret == DB_NOTFOUND) return;
    print_error(__FILE__, __LINE__, "(db) db_delete('%s'), err: %d, %s", word, ret, db_strerror(ret));
    exit(2);
}

static int db_get_dbvalue(void *vhandle, const char *word, /*@out@*/ dbv_t *val){
  int ret;
  DBT db_key;
  DBT db_data;
  char *t;
  uint32_t cv[2] = { 0l, 0l };

  dbh_t *handle = vhandle;

  db_enforce_locking(handle, "db_getvalue");

  DBT_init(db_key);
  DBT_init(db_data);

  db_key.data = t = xstrdup(word);
  db_key.size = strlen(word);

  db_data.data = &cv;
  db_data.size = sizeof(cv);
  db_data.ulen = sizeof(cv);
  db_data.flags = DB_DBT_USERMEM; /* saves the memcpy */

  ret = handle->dbp->get(handle->dbp, NULL, &db_key, &db_data, 0);
  xfree(t);
  switch (ret) {
  case 0:
      /* we used to do this: but DB_DBT_USERMEM saves us the copy
      memcpy(cv, db_data.data, db_data.size);
      */
    if (!handle->is_swapped){		/* convert from struct to array */
      val->count = cv[0];
      val->date  = cv[1];
    } else {
      val->count = swap_32bit(cv[0]);
      val->date  = swap_32bit(cv[1]);
    }
    break;
  case DB_NOTFOUND:
    if (DEBUG_DATABASE(2)) {
      fprintf(dbgout, "[%lu] db_getvalue (%s): [%s] not found\n", (unsigned long) handle->pid, handle->name, word);
    }
    break;
  default:
    print_error(__FILE__, __LINE__, "(db) db_getvalue( '%s' ), err: %d, %s", word, ret, db_strerror(ret));
    exit(2);
  }
  return ret;
}


/*
Store VALUE in database, using WORD as database key
Notes: Calls exit if an error occurs.
*/
void db_setvalue(void *vhandle, const char * word, uint32_t count){
  dbv_t val;
  val.count = count;
  val.date  = today;		/* date in form YYYYMMDD */
  db_set_dbvalue(vhandle, word, &val);
}


static void db_set_dbvalue(void *vhandle, const char * word, dbv_t *val){
  int ret;
  DBT db_key;
  DBT db_data;
  uint32_t cv[2];
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
      cv[0] = swap_32bit(val->count);
      cv[1] = swap_32bit(val->date);
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
      fprintf(dbgout, "db_set_dbvalue (%s): [%s] has value %lu\n", handle->name, word, (unsigned long)val->count);
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
void db_increment(void *vhandle, const char *word, uint32_t value){
    value += db_getvalue(vhandle, word);
    db_setvalue(vhandle, word, value);
}

/*
  Decrement count associated with WORD by VALUE,
  if WORD exists in the database.
*/
void db_decrement(void *vhandle, const char *word, uint32_t value){
    uint32_t dv = db_getvalue(vhandle, word);
    value = dv < value ? 0 : dv - value;
    db_setvalue(vhandle, word, value);
}

/*
  Get the number of messages associated with database.
*/
uint32_t db_get_msgcount(void *vhandle){
  return db_getvalue(vhandle, MSG_COUNT_TOK);
}

/*
 Set the number of messages associated with database.
*/
void db_set_msgcount(void *vhandle, uint32_t count){
  db_setvalue(vhandle, MSG_COUNT_TOK, count);
}


/* Close files and clean up. */
void db_close(void *vhandle){
  dbh_t *handle = vhandle;
  int ret;

  if (handle == NULL) return;
  if (handle->fd >= 0) {
      db_lock(handle->fd, F_UNLCK,
	      (short int)(handle->open_mode == DB_READ ? F_RDLCK : F_WRLCK));
  }
  if ((ret = handle->dbp->close(handle->dbp, 0))) {
    print_error(__FILE__, __LINE__, "(db) db_close err: %d, %s", ret, db_strerror(ret));
  }

  if (DEBUG_DATABASE(1)) {
      fprintf(dbgout, "db_close( %s, %s )\n", handle->name, handle->filename);
  }

/*  db_lock_release(handle); */
  dbh_free(handle);
}

/*
 flush any data in memory to disk
*/
void db_flush(void *vhandle){
    dbh_t *handle = vhandle;
    int ret;
    if ((ret = handle->dbp->sync(handle->dbp, 0))) {
	print_error(__FILE__, __LINE__, "(db) db_sync: err: %d, %s", ret, db_strerror(ret));
    }
}

/* implements locking. */
static int db_lock(int fd, int cmd, short int type){
    struct flock lock;

    lock.l_type = type;
    lock.l_start = 0;
    lock.l_whence = (short int)SEEK_SET;
    lock.l_len = 0;
    return (fcntl(fd, cmd, &lock));
}

int db_foreach(void *vhandle, 
	       int (*hook)(char *key,   uint32_t key_size,
			   char *value, uint32_t key_value, 
			   void *userdata), void *userdata) {
    dbh_t *handle = vhandle;
    int ret;
    DBC dbc;
    DBC *dbcp = &dbc;
    DBT key, data;
    memset (&key, 0, sizeof(key));
    memset (&data, 0, sizeof(data));

    ret = handle->dbp->cursor(handle->dbp, NULL, &dbcp, 0);
    if (ret) {
	handle->dbp->err(handle->dbp, ret, "(cursor): %s", handle->filename);
	return -1;
    }

    while((ret = dbcp->c_get(dbcp, &key, &data, DB_NEXT)) == 0) {
	uint32_t cv[2];

	memcpy(&cv, data.data, data.size);
	if (handle->is_swapped) {
	    unsigned int i;
	    for(i=0;i< (data.size / 4);i++)
		cv[i] = swap_32bit(cv[i]);
	}

	/* call user function */
	if (hook(key.data, key.size, data.data, data.size, userdata))
	    break;
    }
    switch(ret) {
	case DB_NOTFOUND:
	    /* OK */
	    ret = 0;
	    break;
	default:
	    handle->dbp->err(handle->dbp, ret, "(c_get)");
	    ret = -1;
    }
    if (dbcp->c_close(dbcp)) {
	    handle->dbp->err(handle->dbp, ret, "(c_close)");
	    ret = -1;
    }
    return ret;
}
