/* $Id$ */

/*****************************************************************************

NAME:
datastore_db.c -- implements the datastore, using Berkeley DB

AUTHORS:
Gyepi Sam <gyepi@praxis-sw.com>   2002 - 2003
Matthias Andree <matthias.andree@gmx.de> 2003

******************************************************************************/

#include "common.h"

#include <db.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/resource.h>

#if NEEDTRIO
#include <trio.h>
#endif

#include "datastore.h"
#include "datastore_db.h"
#include "error.h"
#include "maint.h"
#include "paths.h"		/* for build_path */
#include "swap.h"
#include "word.h"
#include "xmalloc.h"
#include "xstrdup.h"

typedef struct {
    char	*path;
    size_t	count;
    char	*name[IX_SIZE];
    int		fd[IX_SIZE];
    dbmode_t	open_mode;
    DB		*dbp[IX_SIZE];
    bool	locked;
    bool	is_swapped;
} dbh_t;

#define DBT_init(dbt) do { memset(&dbt, 0, sizeof(DBT)); } while(0)

#define DB_AT_LEAST(maj, min) ((DB_VERSION_MAJOR > (maj)) || ((DB_VERSION_MAJOR == (maj)) && (DB_VERSION_MINOR >= (min))))

#if DB_AT_LEAST(4,1)
#define	DB_OPEN(db, file, database, dbtype, flags, mode) db->open(db, NULL /*txnid*/, file, database, dbtype, flags, mode)
#else
#define	DB_OPEN(db, file, database, dbtype, flags, mode) db->open(db, file, database, dbtype, flags, mode)
#endif

/* Function prototypes */

/* Function definitions */

const char *db_version_str(void)
{
    static char v[80];
    snprintf(v, sizeof(v), "BerkeleyDB (%d.%d.%d)",
	    DB_VERSION_MAJOR, DB_VERSION_MINOR, DB_VERSION_PATCH);
    return v;
}


static void db_enforce_locking(dbh_t *handle, const char *func_name)
{
    if (handle->locked == false) {
	print_error(__FILE__, __LINE__, "%s (%s): Attempt to access unlocked handle.", func_name, handle->name[0]);
	exit(EX_ERROR);
    }
}


/* implements locking. */
static int db_lock(int fd, int cmd, short int type)
{
    struct flock lock;

    lock.l_type = type;
    lock.l_start = 0;
    lock.l_whence = (short int)SEEK_SET;
    lock.l_len = 0;
    return (fcntl(fd, cmd, &lock));
}


static dbh_t *dbh_init(const char *path, size_t count, const char **names)
{
    size_t c;
    dbh_t *handle;

    handle = xmalloc(sizeof(dbh_t));
    memset(handle, 0, sizeof(dbh_t));	/* valgrind */

    handle->count = count;
    handle->path = xstrdup(path);
    for (c = 0; c < count ; c += 1) {
	size_t len = strlen(path) + strlen(names[c]) + 2;
	handle->name[c] = xmalloc(len);
	handle->fd[c]    = -1; /* for lock */
	build_path(handle->name[c], len, path, names[c]);
    }
    handle->locked  = false;
    handle->is_swapped= false;

    return handle;
}


static void dbh_free(/*@only@*/ dbh_t *handle)
{
    if (handle != NULL) {
	size_t c;
	for (c = 0; c < handle->count ; c += 1)
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


static void check_db_version(void)
{
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
	    exit(EX_ERROR);
	}
    }
}

/** check limit of open file (given through descriptor \a fd) against
 * current resource limit and warn if file size is "close" (2 MB) to the
 * limit. errors from the system are ignored, no warning then.
 */
static void check_fsize_limit(int fd, uint32_t pagesize) {
    struct stat st;
    struct rlimit rl;

    if (fstat(fd, &st)) return; /* ignore error */
    if (getrlimit(RLIMIT_FSIZE, &rl)) return; /* ignore error */
    if (rl.rlim_cur != (rlim_t)RLIM_INFINITY) {
	/* WARNING: Be extremely careful that in these comparisons there
	 * is no unsigned term, it will spoil everything as C will
	 * coerce into unsigned types, which would then make "file size
	 * larger than resource limit" undetectable. BUG: this doesn't
	 * work when pagesize doesn't fit into signed long. ("requires"
	 * 2**31 for file size and 32-bit integers to fail) */
	if ((off_t)(rl.rlim_cur/pagesize) - st.st_size/(long)pagesize < 16) {
	    print_error(__FILE__, __LINE__, "error: the data base file size is only 16 pages");
	    print_error(__FILE__, __LINE__, "       below the resource limit. Cowardly refusing");
	    print_error(__FILE__, __LINE__, "       to continue to avoid data base corruption.");
	    exit(EX_ERROR);
	}
	if ((off_t)(rl.rlim_cur >> 20) - (st.st_size >> 20) < 2) {
	    print_error(__FILE__, __LINE__, "warning: data base file size approaches resource limit.");
	    print_error(__FILE__, __LINE__, "         write errors (bumping into the limit) can cause");
	    print_error(__FILE__, __LINE__, "         data base corruption.");
	}
    }
}


/*
  Initialize database.
  Returns: pointer to database handle on success, NULL otherwise.
*/
void *db_open(const char *db_file, size_t count, const char **names, dbmode_t open_mode)
{
    int ret;
    int is_swapped;

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
   
    check_db_version();

    if (open_mode == DB_READ)
	opt_flags = DB_RDONLY;
    else
	/* Read-write mode implied.  Allow database to be created if
	 * necessary. DB_EXCL makes sure out locking doesn't fail if two
	 * applications try to create a DB at the same time. */
	opt_flags = 0;

    for (idx = 0; idx < COUNTOF(retryflags); idx += 1) {
	size_t i;
	uint32_t retryflag = retryflags[idx], pagesize;
	DB_BTREE_STAT *dbstat = NULL;

	handle = dbh_init(db_file, count, names);

	if (handle == NULL)
	    break;

	for (i = 0; handle && i < handle->count; i += 1) {
	    DB *dbp;

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

	    if ((ret = DB_OPEN(dbp, handle->name[i], NULL, DB_BTREE, opt_flags | retryflag, 0664)) != 0 && (ret != ENOENT ||
		(ret = DB_OPEN(dbp, handle->name[i], NULL, DB_BTREE, opt_flags | DB_CREATE | DB_EXCL | retryflag, 0664)) != 0)) {
		if (DEBUG_DATABASE(1))
		    print_error(__FILE__, __LINE__, "(db) open( %s ), err: %d, %s",
				handle->name[i], ret, db_strerror(ret));
		goto open_err;
	    }
	    
	    if (DEBUG_DATABASE(1))
		fprintf(dbgout, "db_open( %s, %d ) -> %d\n", handle->name[i], open_mode, ret);

	    /* see if the database byte order differs from that of the cpu's */
#if DB_AT_LEAST(3,3)
	    ret = dbp->get_byteswapped (dbp, &is_swapped);
#else
	    ret = 0;
	    is_swapped = dbp->get_byteswapped (dbp);
#endif
	    handle->is_swapped = is_swapped ? true : false;

	    if (ret != 0) {
		dbp->err (dbp, ret, "%s (db) get_byteswapped: %s",
			  progname, handle->name);
		db_close(handle, false);
		return NULL;		/* handle already freed, ok to return */
	    }

	    ret = dbp->fd(dbp, &handle->fd[i]);
	    if (ret < 0) {
		dbp->err (dbp, ret, "%s (db) fd: %s",
			  progname, handle->name);
		db_close(handle, false);
		return NULL;		/* handle already freed, ok to return */
	    }

	    /* query page size */
#if DB_AT_LEAST(3,3)
	    ret = dbp->stat(dbp, &dbstat, DB_FAST_STAT);
#else
	    ret = dbp->stat(dbp, &dbstat, NULL, DB_CACHED_COUNTS);
#endif
      
	    if (ret) {
		dbp->err (dbp, ret, "%s (db) stat: %s", progname, handle->name);
		db_close(handle, false);
		return NULL;
	    }
	    pagesize = dbstat->bt_pagesize;
	    free(dbstat);

	    check_fsize_limit(handle->fd[i], pagesize);

	    if (db_lock(handle->fd[i], F_SETLK,
			(short int)(open_mode == DB_READ ? F_RDLCK : F_WRLCK)))
	    {
		int e = errno;
		handle->fd[i] = -1;
		db_close(handle, true);
		handle = NULL;	/* db_close freed it, we don't want to use it anymore */
		errno = e;
		/* do not bother to retry if the problem wasn't EAGAIN */
		if (e != EAGAIN && e != EACCES) return NULL;
		/* do not goto open_err here, db_close frees the handle! */
		if (errno == EACCES)
		    errno = EAGAIN;
	    } else {
		idx = COUNTOF(retryflags);
	    }
	} /* for i over handle */
    } /* for idx over retryflags */

    if (handle) {
	unsigned int i;
	dsh_t *dsh;
	handle->locked = true;
	for (i = 0; i < handle->count; i += 1) {
	    if (handle->fd[i] < 0)
		handle->locked=false;
	}
	dsh = dsh_init(handle, handle->count, handle->is_swapped);
	return (void *)dsh;
    }

    return NULL;

 open_err:
    dbh_free(handle);

    return NULL;
}


int db_delete(dsh_t *dsh, const dbv_t *token)
{
    int ret = 0;
    size_t i;
    dbh_t *handle = dsh->dbh;

    DBT db_key;
    DBT_init(db_key);

    db_key.data = token->data;
    db_key.size = token->leng;

    for (i = 0; i < handle->count; i += 1) {
	DB *dbp = handle->dbp[i];
	ret = dbp->del(dbp, NULL, &db_key, 0);

	if (ret != 0 && ret != DB_NOTFOUND) {
	    print_error(__FILE__, __LINE__, "(db) db_delete('%.*s'), err: %d, %s", 
		    CLAMP_INT_MAX(db_key.size),
		    (const char *) db_key.data,
    		    ret, db_strerror(ret));
	    exit(EX_ERROR);
	}
    }

    return ret;		/* 0 if ok */
}


int db_get_dbvalue(dsh_t *dsh, const dbv_t *token, /*@out@*/ dbv_t *val)
{
    int ret = 0;
    DBT db_key;
    DBT db_data;

    dbh_t *handle = dsh->dbh;
    DB *dbp = handle->dbp[dsh->index];

    db_enforce_locking(handle, "db_get_dbvalue");

    DBT_init(db_key);
    DBT_init(db_data);

    db_key.data = token->data;
    db_key.size = token->leng;

    db_data.data = val->data;
    db_data.size = val->size;		/* cur used */
    db_data.ulen = val->size;		/* max size */
    db_data.flags = DB_DBT_USERMEM;	/* saves the memcpy */

    ret = dbp->get(dbp, NULL, &db_key, &db_data, 0);

    if (val->size < db_data.size) {
	print_error(__FILE__, __LINE__, "(db) db_get_dbvalue( '%.*s' ), size error %lu::%lu",
		    CLAMP_INT_MAX(token->leng), (char *)token->data,
		    (unsigned long)val->leng,
		    (unsigned long)db_data.size);
	exit(EX_ERROR);
    }

    val->leng = db_data.size;		/* read count */

    switch (ret) {
    case 0:
	break;
    case DB_NOTFOUND:
	ret = DS_NOTFOUND;
	if (DEBUG_DATABASE(3)) {
	    fprintf(dbgout, "db_get_dbvalue: [%.*s] not found\n", 
		    CLAMP_INT_MAX(token->leng), (char *) token->data);
	}
	break;
    default:
	print_error(__FILE__, __LINE__, "(db) db_get_dbvalue( '%.*s' ), err: %d, %s", 
		    CLAMP_INT_MAX(token->leng), (char *) token->data, ret, db_strerror(ret));
	exit(EX_ERROR);
    }

    return ret;
}


int db_set_dbvalue(dsh_t *dsh, const dbv_t *token, dbv_t *val)
{
    int ret;

    DBT db_key;
    DBT db_data;

    dbh_t *handle = dsh->dbh;
    DB *dbp = handle->dbp[dsh->index];

    db_enforce_locking(handle, "db_set_dbvalue");

    DBT_init(db_key);
    DBT_init(db_data);

    db_key.data = token->data;
    db_key.size = token->leng;

    db_data.data = val->data;
    db_data.size = val->leng;		/* write count */

    ret = dbp->put(dbp, NULL, &db_key, &db_data, 0);

    if (ret != 0) {
	print_error(__FILE__, __LINE__, "(db) db_set_dbvalue( '%.*s' ), err: %d, %s", 
		    CLAMP_INT_MAX(token->leng), (char *)token->data, ret, db_strerror(ret));
	exit(EX_ERROR);
    }

    return 0;
}


/* Close files and clean up. */
void db_close(void *vhandle, bool nosync)
{
    dbh_t *handle = vhandle;
    int ret;
    size_t i;
    uint32_t f = nosync ? DB_NOSYNC : 0;

    if (DEBUG_DATABASE(1)) {
	dbh_print_names(handle, "db_close");
	fprintf(dbgout, " %s\n", nosync ? "nosync" : "sync");
    }

    if (handle == NULL) return;

#if 0
    if (handle->fd >= 0) {
	ds_lock(handle->fd, F_UNLCK,
		(short int)(handle->open_mode == DB_READ ? F_RDLCK : F_WRLCK));
    }
#endif

    for (i = 0; i < handle->count; i += 1) {
	DB *dbp = handle->dbp[i];
	if (dbp == NULL)
	    continue;
	if ((ret = dbp->close(dbp, f)))
	    print_error(__FILE__, __LINE__, "(db) db_close err: %d, %s", ret, db_strerror(ret));
    }

/*  ds_lock_release(handle); */
    dbh_free(handle);
}


/*
 flush any data in memory to disk
*/
void db_flush(dsh_t *dsh)
{
    dbh_t *handle = dsh->dbh;
    int ret;
    size_t i;
    for (i = 0; i < handle->count; i += 1) {
	DB *dbp = handle->dbp[i];
	if ((ret = dbp->sync(dbp, 0)))
	    print_error(__FILE__, __LINE__, "(db) db_sync: err: %d, %s", ret, db_strerror(ret));
    }
}


int db_foreach(dsh_t *dsh, db_foreach_t hook, void *userdata)
{
    dbh_t *handle = dsh->dbh;
    DB *dbp = handle->dbp[dsh->index];

    int ret = 0;

    DBC dbc;
    DBC *dbcp = &dbc;
    DBT key, data;

    dbv_t dbv_key, dbv_data;
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    ret = dbp->cursor(dbp, NULL, &dbcp, 0);
    if (ret) {
	dbp->err(dbp, ret, "(cursor): %s", handle->path);
	return -1;
    }

    for (ret =  dbcp->c_get(dbcp, &key, &data, DB_FIRST); 
	 ret == 0;
	 ret =  dbcp->c_get(dbcp, &key, &data, DB_NEXT))
    {
	int rc;

	/* Question: Is there a way to avoid using malloc/free? */

	/* switch to "dbv_t *" variables */
	dbv_key.leng = key.size;
	dbv_key.data = xmalloc(dbv_key.leng+1);
	memcpy(dbv_key.data, key.data, dbv_key.leng);
	((char *)dbv_key.data)[dbv_key.leng] = '\0';

	dbv_data.data = data.data;
	dbv_data.leng = data.size;

	/* call user function */
	rc = hook(&dbv_key, &dbv_data, userdata);
	xfree(dbv_key.data);
	
	/* returns 0 if ok, 1 if not */
	if (rc != 0)
	    break;
    }

    switch (ret) {
    case 0:
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

    return ret;		/* 0 if ok */
}

const char *db_str_err(int e) {
    return db_strerror(e);
}
