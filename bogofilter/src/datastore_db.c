/* $Id$ */

/*****************************************************************************

NAME:
datastore_db.c -- implements the datastore, using Berkeley DB

AUTHORS:
Gyepi Sam <gyepi@praxis-sw.com>   2002 - 2003
Matthias Andree <matthias.andree@gmx.de> 2003

******************************************************************************/

#include "system.h"
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
#include "word.h"
#include "xmalloc.h"
#include "xstrdup.h"

#define MSG_COUNT_TOK ((const byte *)".MSG_COUNT")
word_t  *msg_count_tok;

#undef UINT32_MAX
#define UINT32_MAX 4294967295lu /* 2 ^ 32 - 1 */

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

int db_cachesize = 0;	/* in MB */
#define	DB_CACHE(db, gb, bytes, chunks) db->set_cachesize(db, 0, db_cachesize*1024*1024, chunks)

/* Function prototypes */

static int db_get_dbvalue(void *vhandle, const word_t *word, dbv_t *val);
static void db_set_dbvalue(void *vhandle, const word_t *word, dbv_t *val);
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
    /*
     * If locking fails with EAGAIN, then try without MMAP, fcntl()
     * locking may be forbidden on mmapped files, or mmap may not be
     * available for NFS. Thanks to Piotr Kucharski and Casper Dik,
     * see news:comp.protocols.nfs and the bogofilter mailing list,
     * message #1520, Message-ID: <20030206172016.GS1214@sgh.waw.pl>
     * Date: Thu, 6 Feb 2003 18:20:16 +0100
     */
    uint32_t retryflags[] = { 0, DB_NOMMAP, 0xffffffff };
    uint32_t *retryflag_i;

    assert(dir && *dir);

    if (open_mode == DB_READ)
	opt_flags = DB_RDONLY;
    else
	/* Read-write mode implied.  Allow database to be created if
	 * necessary. DB_EXCL makes sure out locking doesn't fail if two
	 * applications try to create a DB at the same time. */
	opt_flags = 0;

    for(retryflag_i = &retryflags[0] ; *retryflag_i != 0xffffffff; retryflag_i++) {
	handle = dbh_init(db_file, name);
	handle->open_mode = open_mode;
	/* create DB handle */
	if ((ret = db_create (&(handle->dbp), NULL, 0)) != 0) {
	    print_error(__FILE__, __LINE__, "(db) create, err: %d, %s",
		    ret, db_strerror(ret));
	    goto open_err;
	}

	if (db_cachesize != 0 &&
		(ret = DB_CACHE(handle->dbp, 0, db_cachesize, 1)) != 0) {
	    print_error(__FILE__, __LINE__, "(db) setcache( %s ), err: %d, %s",
		    db_file, ret, db_strerror(ret));
	    goto open_err; 
	}

	/* open data base */
	if ((ret = DB_OPEN(handle->dbp, db_file, NULL, DB_BTREE, opt_flags | *retryflag_i, 0664)) != 0 &&
		(ret = DB_OPEN(handle->dbp, db_file, NULL, DB_BTREE, opt_flags |  DB_CREATE | DB_EXCL | *retryflag_i, 0664)) != 0) {
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
	    db_close(handle, false);
	    goto open_err;
	}

	ret = handle->dbp->fd(handle->dbp, &handle->fd);
	if (ret < 0) {
	    handle->dbp->err (handle->dbp, ret, "%s (db) fd: %s",
		    progname, db_file);
	    db_close(handle, false);
	    goto open_err;
	}

	if (db_lock(handle->fd, F_SETLK,
		    (short int)(open_mode == DB_READ ? F_RDLCK : F_WRLCK)))
	{
	    int e = errno;
	    handle->fd = -1;
	    db_close(handle, true);
	    errno = e;
	    /* do not bother to retry if the problem wasn't EAGAIN */
	    if (e != EAGAIN) return NULL;
	    /* do not goto open_err here, db_close frees the handle! */
	} else {
	    break;
	}
    }

    if (handle -> fd >= 0) {
	handle->locked = true;
	return (void *)handle;
    }
    return NULL;

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
uint32_t db_getvalue(void *vhandle, const word_t *word){
  dbv_t val;
  int ret;
  uint32_t value = 0;
  dbh_t *handle = vhandle;

  ret = db_get_dbvalue(vhandle, word, &val);

  if (ret == 0) {
    value = val.count;

    if (DEBUG_DATABASE(2)) {
      fprintf(dbgout, "[%lu] db_getvalue (%s): [",
	      (unsigned long) handle->pid, handle->name);
      word_puts(word, 0, dbgout);
      fprintf(dbgout, "] has value %lu\n",
	      (unsigned long)value);
    }
    return value;
  } else {
    return 0;
  }
}

void db_delete(void *vhandle, const word_t *word) {
    int ret;
    dbh_t *handle = vhandle;

    DBT db_key;
    DBT_init(db_key);

    db_key.data = word->text;
    db_key.size = word->leng;

    ret = handle->dbp->del(handle->dbp, NULL, &db_key, 0);

    if (ret == 0 || ret == DB_NOTFOUND) return;
    print_error(__FILE__, __LINE__, "(db) db_delete('%s'), err: %d, %s", word->text, ret, db_strerror(ret));
    exit(2);
}

static int db_get_dbvalue(void *vhandle, const word_t *word, /*@out@*/ dbv_t *val){
  int ret;
  DBT db_key;
  DBT db_data;
  uint32_t cv[2] = { 0l, 0l };

  dbh_t *handle = vhandle;

  db_enforce_locking(handle, "db_getvalue");

  DBT_init(db_key);
  DBT_init(db_data);

  db_key.data = word->text;
  db_key.size = word->leng;

  db_data.data = &cv;
  db_data.size = sizeof(cv);
  db_data.ulen = sizeof(cv);
  db_data.flags = DB_DBT_USERMEM; /* saves the memcpy */

  ret = handle->dbp->get(handle->dbp, NULL, &db_key, &db_data, 0);

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
      fprintf(dbgout, "[%lu] db_getvalue (%s): [", (unsigned long) handle->pid, handle->name);
      word_puts(word, 0, dbgout);
      fputs("] not found\n", dbgout);
    }
    break;
  default:
    print_error(__FILE__, __LINE__, "(db) db_getvalue( '%s' ), err: %d, %s", word->text, ret, db_strerror(ret));
    exit(2);
  }
  return ret;
}


/*
Store VALUE in database, using WORD as database key
Notes: Calls exit if an error occurs.
*/
void db_setvalue(void *vhandle, const word_t *word, uint32_t count){
  dbv_t val;
  val.count = count;
  val.date  = today;		/* date in form YYYYMMDD */
  db_set_dbvalue(vhandle, word, &val);
}


static void db_set_dbvalue(void *vhandle, const word_t *word, dbv_t *val){
  int ret;
  DBT db_key;
  DBT db_data;
  uint32_t cv[2];
  dbh_t *handle = vhandle;

  db_enforce_locking(handle, "db_set_dbvalue");

  DBT_init(db_key);
  DBT_init(db_data);

  db_key.data = word->text;
  db_key.size = word->leng;

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

  if (ret == 0){
    if (DEBUG_DATABASE(2)) {
      fprintf(dbgout, "db_set_dbvalue (%s): [",
	      handle->name);
      word_puts(word, 0, dbgout);
      fprintf(dbgout, "] has value %lu\n",
	      (unsigned long)val->count);
    }
  }
  else {
    print_error(__FILE__, __LINE__, "(db) db_set_dbvalue( '%s' ), err: %d, %s", word->text, ret, db_strerror(ret));
    exit(2);
  }
}


/*
  Increment count associated with WORD, by VALUE.
 */
void db_increment(void *vhandle, const word_t *word, uint32_t value){
    uint32_t dv = db_getvalue(vhandle, word);
    value = UINT32_MAX - dv < value ? UINT32_MAX : dv + value;
    db_setvalue(vhandle, word, value);
}

/*
  Decrement count associated with WORD by VALUE,
  if WORD exists in the database.
*/
void db_decrement(void *vhandle, const word_t *word, uint32_t value){
    uint32_t dv = db_getvalue(vhandle, word);
    value = dv < value ? 0 : dv - value;
    db_setvalue(vhandle, word, value);
}

/*
  Get the number of messages associated with database.
*/
uint32_t db_get_msgcount(void *vhandle){
    if (msg_count_tok == NULL)
	msg_count_tok = word_new(MSG_COUNT_TOK, strlen((const char *)MSG_COUNT_TOK));
    return db_getvalue(vhandle, msg_count_tok);
}

/*
 Set the number of messages associated with database.
*/
void db_set_msgcount(void *vhandle, uint32_t count){
    db_setvalue(vhandle, msg_count_tok, count);
}


/* Close files and clean up. */
void db_close(void *vhandle, bool nosync){
  dbh_t *handle = vhandle;
  int ret;
  uint32_t f = nosync ? DB_NOSYNC : 0;

  if (DEBUG_DATABASE(1)) {
      fprintf(dbgout, "db_close(%s, %s, %s)\n", handle->name, handle->filename,
	      nosync ? "true" : "false");
  }

  if (handle == NULL) return;
#if 0
  if (handle->fd >= 0) {
      db_lock(handle->fd, F_UNLCK,
	      (short int)(handle->open_mode == DB_READ ? F_RDLCK : F_WRLCK));
  }
#endif
  if ((ret = handle->dbp->close(handle->dbp, f))) {
    print_error(__FILE__, __LINE__, "(db) db_close err: %d, %s", ret, db_strerror(ret));
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

int db_foreach(void *vhandle, db_foreach_t hook, void *userdata) {
    dbh_t *handle = vhandle;
    int ret;
    DBC dbc;
    DBC *dbcp = &dbc;
    DBT key, data;
    word_t w_key, w_data;
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

	/* switch to "word_t *" variables */
	w_key.text = key.data;
	w_key.leng = key.size;
	w_data.text = data.data;
	w_data.leng = data.size;

	/* call user function */
	if (hook(&w_key, &w_data, userdata))
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
