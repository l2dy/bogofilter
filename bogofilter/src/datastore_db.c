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
#include <string.h>
#include <stdlib.h>
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

typedef struct {
    char *filename;
    size_t count;    
    char *name[2];
    int fd;
    dbmode_t open_mode;
    DB *dbp[2];
    pid_t pid;
    bool locked;
    int  is_swapped;
    /*@owned@*/ void *dbh_word;	/* database handle */
    /*@owned@*/ void *dbh_good;	/* database handle */
    /*@owned@*/ void *dbh_spam;	/* database handle */
} dbh_t;

#define DBT_init(dbt) do { memset(&dbt, 0, sizeof(DBT)); } while(0)

#define DB_AT_LEAST(maj, min) ((DB_VERSION_MAJOR > (maj)) || ((DB_VERSION_MAJOR == (maj)) && (DB_VERSION_MINOR >= (min))))

#if DB_AT_LEAST(4,1)
#define	DB_OPEN(db, file, database, dbtype, flags, mode) db->open(db, NULL /*txnid*/, file, database, dbtype, flags, mode)
#else
#define	DB_OPEN(db, file, database, dbtype, flags, mode) db->open(db, file, database, dbtype, flags, mode)
#endif

int db_cachesize = 0;	/* in MB */

/* Function prototypes */

/* Function definitions */

static void db_enforce_locking(dbh_t *handle, const char *func_name)
{
    if (handle->locked == false) {
	print_error(__FILE__, __LINE__, "%s (%s): Attempt to access unlocked handle.", func_name, handle->name[0]);
	exit(2);
    }
}


static dbh_t *dbh_init(const char *path, size_t count, const char **names)
{
    size_t c;
    dbh_t *handle;

    handle = xmalloc(sizeof(dbh_t));
    memset(handle, 0, sizeof(dbh_t));	/* valgrind */

    handle->count = count;
    handle->filename = xstrdup(path);
    for (c = 0; c < count ; c += 1) {
	size_t len = strlen(path) + strlen(names[c]) + 2;
	handle->name[c] = xmalloc(len);
	build_path(handle->name[c], len, path, names[c]);
    }
    handle->pid	    = getpid();
    handle->locked  = false;
    handle->is_swapped= 0;
    handle->fd	    = -1; /* for lock */

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
    size_t idx;
    uint32_t retryflags[] = { 0, DB_NOMMAP };
    int maj, min;
    static int version_ok;
    
    if (!version_ok) {
	version_ok = 1;
	(void)db_version(&maj, &min, NULL);
	if (!(maj == DB_VERSION_MAJOR && min == DB_VERSION_MINOR)) {
	    fprintf(stderr, "The DB versions do not match.\n"
		    "This program was compiled for DB version %d.%d,\n"
		    "but it is linked against DB version %d.%d.\nAborting.\n",
		    DB_VERSION_MAJOR, DB_VERSION_MINOR, maj, min);
	    exit(2);
	}
    }

    if (open_mode == DB_READ)
	opt_flags = DB_RDONLY;
    else
	/* Read-write mode implied.  Allow database to be created if
	 * necessary. DB_EXCL makes sure out locking doesn't fail if two
	 * applications try to create a DB at the same time. */
	opt_flags = 0;

    for (idx = 0; idx < COUNTOF(retryflags); idx += 1) {
	size_t i;
	uint32_t retryflag = retryflags[idx];
	handle = dbh_init(db_file, count, names);

	if (handle == NULL)
	    break;

	for (i = 0; i < handle->count; i += 1) {
	    DB *dbp = handle->dbp[i];
	    /* create DB handle */
	    if ((ret = db_create (&dbp, NULL, 0)) != 0) {
		print_error(__FILE__, __LINE__, "(db) create, err: %d, %s",
			    ret, db_strerror(ret));
		goto open_err;
	    }

	    handle->dbp[i] = dbp;

	    if (db_cachesize != 0 &&
		(ret = dbp->set_cachesize(dbp, db_cachesize/1024, (db_cachesize % 1024) * 1024*1024, 1)) != 0) {
		print_error(__FILE__, __LINE__, "(db) setcache( %s ), err: %d, %s",
			    handle->name[0], ret, db_strerror(ret));
		goto open_err; 
	    }

	    /* open data base */

	    if ((ret = DB_OPEN(dbp, handle->name[i], NULL, DB_BTREE, opt_flags | retryflag, 0664)) != 0 &&
		(ret = DB_OPEN(dbp, handle->name[i], NULL, DB_BTREE, opt_flags | DB_CREATE | DB_EXCL | retryflag, 0664)) != 0) {
/*
  print_error(__FILE__, __LINE__, "(db) open( %s ), err: %d, %s",
  handle->name[i], ret, db_strerror(ret));
*/
		goto open_err;
	    }
	    
	    if (DEBUG_DATABASE(1))
		fprintf(dbgout, "db_open( %s, %d ) -> %d\n", handle->name[i], open_mode, ret);

	    /* see if the database byte order differs from that of the cpu's */
#if DB_AT_LEAST(3,3)
	    ret = dbp->get_byteswapped (dbp, &(handle->is_swapped));
#else
	    ret = 0;
	    handle->is_swapped = dbp->get_byteswapped (dbp);
#endif
	    if (ret != 0) {
		dbp->err (dbp, ret, "%s (db) get_byteswapped: %s",
			  progname, handle->name);
		db_close(handle, false);
		return NULL;		/* handle already freed, ok to return */
	    }

	    ret = dbp->fd(dbp, &handle->fd);
	    if (ret < 0) {
		dbp->err (dbp, ret, "%s (db) fd: %s",
			  progname, handle->name);
		db_close(handle, false);
		return NULL;		/* handle already freed, ok to return */
	    }
	}

	if (db_lock(handle->fd, F_SETLK,
		    (short int)(open_mode == DB_READ ? F_RDLCK : F_WRLCK)))
	{
	    int e = errno;
	    handle->fd = -1;
	    db_close(handle, true);
	    handle = NULL;	/* db_close freed it, we don't want to use it anymore */
	    errno = e;
	    /* do not bother to retry if the problem wasn't EAGAIN */
	    if (e != EAGAIN && e != EACCES) return NULL;
	    /* do not goto open_err here, db_close frees the handle! */
	} else {
	    break;
	}
    }

    if (handle && (handle->fd >= 0)) {
	handle->locked = true;
	return (void *)handle;
    }

    return NULL;

 open_err:
    dbh_free(handle);
    
    return NULL;
}


void db_delete(void *vhandle, const word_t *word)
{
    int ret;
    size_t i;
    dbh_t *handle = vhandle;

    DBT db_key;
    DBT_init(db_key);

    db_key.data = word->text;
    db_key.size = word->leng;

    for (i = 0; i < handle->count; i += 1) {
	DB *dbp = handle->dbp[i];
	ret = dbp->del(dbp, NULL, &db_key, 0);

	if (ret != 0 && ret != DB_NOTFOUND) {
	    print_error(__FILE__, __LINE__, "(db) db_delete('%s'), err: %d, %s", word->text, ret, db_strerror(ret));
	    exit(2);
	}
    }

    return;
}


int db_get_dbvalue(void *vhandle, const word_t *word, /*@out@*/ dbv_t *val)
{
    int ret;
    bool found = false;
    size_t i;
    DBT db_key;
    DBT db_data;
    uint32_t cv[3] = { 0l, 0l, 0l };

    dbh_t *handle = vhandle;

    db_enforce_locking(handle, "db_get_dbvalue");

    DBT_init(db_key);
    DBT_init(db_data);

    db_key.data = word->text;
    db_key.size = word->leng;

    db_data.data = &cv;
    db_data.size = sizeof(cv);
    db_data.ulen = sizeof(cv);
    db_data.flags = DB_DBT_USERMEM; /* saves the memcpy */

    for (i = 0; i < handle->count; i += 1) {
	DB *dbp = handle->dbp[i];
	ret = dbp->get(dbp, NULL, &db_key, &db_data, 0);

	switch (ret) {
	case 0:
	    found = true;
	    /* we used to do this: but DB_DBT_USERMEM saves us the copy
	       memcpy(cv, db_data.data, db_data.size);
	    */
	    if (handle->count == 1) {
		if (!handle->is_swapped) {		/* convert from struct to array */
		    val->spamcount = cv[0];
		    val->goodcount = cv[1];
		    val->date      = cv[2];
		} else {
		    val->spamcount = swap_32bit(cv[0]);
		    val->goodcount = swap_32bit(cv[1]);
		    val->date      = swap_32bit(cv[2]);
		}
	    } else {
		uint32_t date;
		if (!handle->is_swapped) {		/* load from struct into array */
		    val->count[i] = cv[0];
		    date          = cv[1];
		} else {
		    val->count[i] = swap_32bit(cv[0]);
		    date          = swap_32bit(cv[1]);
		}
		val->date = max(val->date, date);
	    }
	    break;
	case DB_NOTFOUND:
	    if (handle->count != 1)
		val->count[i] = 0;
	    if (DEBUG_DATABASE(3)) {
		fprintf(dbgout, "db_get_dbvalue (%s): [", handle->name[i]);
		word_puts(word, 0, dbgout);
		fputs("] not found\n", dbgout);
	    }
	    break;
	default:
	    print_error(__FILE__, __LINE__, "(db) db_get_dbvalue( '%s' ), err: %d, %s", word->text, ret, db_strerror(ret));
	    exit(2);
	}
    }
    if (!found &&DEBUG_DATABASE(3)) {
	fprintf(dbgout, "db_getvalues (%s): [",
		handle->name[0]);
	word_puts(word, 0, dbgout);
	fprintf(dbgout, "] has values %lu,%lu\n",
		(unsigned long)val->spamcount,
		(unsigned long)val->goodcount);
    }

    return found ? 0 : DB_NOTFOUND;
}


void db_set_dbvalue(void *vhandle, const word_t *word, dbv_t *val)
{
    int ret;
    DBT db_key;
    DBT db_data;
    size_t i;
    uint32_t cv[3];
    dbh_t *handle = vhandle;

    db_enforce_locking(handle, "db_set_dbvalue");

    DBT_init(db_key);
    DBT_init(db_data);

    db_key.data = word->text;
    db_key.size = word->leng;

    for (i = 0; i < handle->count; i += 1) {
	DB *dbp = handle->dbp[i];
	if (handle->count == 1) {
	    if (!handle->is_swapped) {		/* convert from struct to array */
		cv[0] = val->spamcount;
		cv[1] = val->goodcount;
		cv[2] = val->date;
	    } else {
		cv[0] = swap_32bit(val->spamcount);
		cv[1] = swap_32bit(val->goodcount);
		cv[2] = swap_32bit(val->date);
	    }
	} else {
	    if (!handle->is_swapped) {		/* convert from struct to array */
		cv[0] = val->count[i];
		cv[1] = val->date;
	    } else {
		cv[0] = swap_32bit(val->count[i]);
		cv[1] = swap_32bit(val->date);
	    }

	}
	db_data.data = &cv;			/* and save array in wordlist */
	if (!datestamp_tokens || val->date == 0)
	    db_data.size = db_data.ulen = 2 * sizeof(cv[0]);
	else
	    db_data.size = db_data.ulen = sizeof(cv);

	ret = dbp->put(dbp, NULL, &db_key, &db_data, 0);

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
	    print_error(__FILE__, __LINE__, "(db) db_set_dbvalue( '%s' ), err: %d, %s", word->text, ret, db_strerror(ret));
	    exit(2);
	}
    }
}


/* Close files and clean up. */
void db_close(void *vhandle, bool nosync)
{
    dbh_t *handle = vhandle;
    int ret;
    size_t i;
    uint32_t f = nosync ? DB_NOSYNC : 0;

    if (DEBUG_DATABASE(1)) {
	dbh_print_names(vhandle, "db_close");
	fprintf(dbgout, " %s\n", nosync ? "nosync" : "sync");
    }

    if (handle == NULL) return;

#if 0
    if (handle->fd >= 0) {
	db_lock(handle->fd, F_UNLCK,
		(short int)(handle->open_mode == DB_READ ? F_RDLCK : F_WRLCK));
    }
#endif

    for (i = 0; i < handle->count; i += 1) {
	DB *dbp = handle->dbp[i];
	if ((ret = dbp->close(dbp, f)))
	    print_error(__FILE__, __LINE__, "(db) db_close err: %d, %s", ret, db_strerror(ret));
    }

/*  db_lock_release(handle); */
    dbh_free(handle);
}


/*
 flush any data in memory to disk
*/
void db_flush(void *vhandle)
{
    dbh_t *handle = vhandle;
    int ret;
    size_t i;
    for (i = 0; i < handle->count; i += 1) {
	DB *dbp = handle->dbp[i];
	if ((ret = dbp->sync(dbp, 0)))
	    print_error(__FILE__, __LINE__, "(db) db_sync: err: %d, %s", ret, db_strerror(ret));
    }
}


int db_foreach(void *vhandle, db_foreach_t hook, void *userdata)
{
    size_t i;
    int ret = 0;
    dbh_t *handle = vhandle;

    for (i = 0; i < handle->count; i += 1)
    {
	DBC dbc;
	DBC *dbcp = &dbc;
	DBT key, data;
	DB *dbp = handle->dbp[i];

	word_t w_key, w_data;
	memset (&key, 0, sizeof(key));
	memset (&data, 0, sizeof(data));

	ret = dbp->cursor(dbp, NULL, &dbcp, 0);
	if (ret) {
	    dbp->err(dbp, ret, "(cursor): %s", handle->filename);
	    return -1;
	}

	while ((ret = dbcp->c_get(dbcp, &key, &data, DB_NEXT)) == 0)
	{
	    uint32_t cv[2];

	    memcpy(&cv, data.data, data.size);
	    if (handle->is_swapped) {
		unsigned int s;
		for(s = 0; s < (data.size / 4); s++)
		    cv[s] = swap_32bit(cv[s]);
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

	switch (ret) {
	case DB_NOTFOUND:
	    /* OK */
	    ret = 0;
	    break;
	default:
	    dbp->err(dbp, ret, "(c_get)");
	    ret = -1;
	}
	if (dbcp->c_close(dbcp)) {
	    dbp->err(dbp, ret, "(c_close)");
	    ret = -1;
	}
    }
    return ret;
}
