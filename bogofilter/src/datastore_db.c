/* $Id$ */

/*****************************************************************************

NAME:
datastore_db.c -- implements the datastore, using Berkeley DB

AUTHORS:
Gyepi Sam <gyepi@praxis-sw.com>   2002 - 2003
Matthias Andree <matthias.andree@gmx.de> 2003 - 2004

******************************************************************************/

/* To avoid header file conflicts the order is:
**	1. System header files
**	2. Header files for external packages
**	3. Bogofilter's header files
*/

/* This code has been tested with BerkeleyDB 3.1 3.2, 3.3, 4.0,
 * 4.1 and 4.2.  -- Matthias Andree, 2004-11-29 */

/* TODO:
 * - implement proper retry when our transaction is aborted after a
 *   deadlock
 * - document code changes
 * - conduct massive tests
 * - check if we really need the log files for "catastrophic recovery"
 *   or if we can remove them (see db_archive documentation)
 *   as the log files are *HUGE* even compared with the data base
 */

/*
 * NOTE: this code is an "#if" nightmare due to the many different APIs
 * in the many different BerkeleyDB versions.
 */

#define DONT_TYPEDEF_SSIZE_T 1
#include "common.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>		/* for SEEK_SET for SunOS 4.1.x */
#include <sys/resource.h>
#include <assert.h>

#include <db.h>
#ifdef NEEDTRIO
#include "trio.h"
#endif

#include "datastore.h"
#include "datastore_db.h"
#include "datastore_dbcommon.h"
#include "bogohome.h"
#include "error.h"
#include "maint.h"
#include "paths.h"		/* for build_path */
#include "rand_sleep.h"
#include "swap.h"
#include "word.h"
#include "xmalloc.h"
#include "xstrdup.h"
#include "mxcat.h"
#include "db_lock.h"

static int lockfd = -1;	/* fd of lock file to prevent concurrent recovery */

/** Default flags for DB_ENV->open() */
static const u_int32_t dbenv_defflags = DB_INIT_MPOOL | DB_INIT_LOCK
				      | DB_INIT_LOG | DB_INIT_TXN;

u_int32_t db_max_locks = 16384;		/* set_lk_max_locks    32768 */
u_int32_t db_max_objects = 16384;	/* set_lk_max_objects  32768 */

#ifdef	FUTURE_DB_OPTIONS
bool	  db_log_autoremove = false;	/* DB_LOG_AUTOREMOVE */
bool	  db_txn_durable = true;	/* not DB_TXN_NOT_DURABLE */
#endif

static const DBTYPE dbtype = DB_BTREE;

#define MAGIC_DBE 0xdbe
#define MAGIC_DBH 0xdb4

/** implementation internal type to keep track of database environments
 * we have opened. */
typedef struct {
    int		magic;
    DB_ENV	*dbe;		/* stores the environment handle */
    char	*directory;	/* stores the home directory for this environment */
} dbe_t;

/** implementation internal type to keep track of databases
 * we have opened. */
typedef struct {
    int		magic;
    char	*path;
    char	*name;
    int		fd;		/* file descriptor of data base file */
    dbmode_t	open_mode;	/* datastore open mode, DS_READ/DS_WRITE */
    DB		*dbp;		/* data base handle */
    bool	is_swapped;	/* set if CPU and data base endianness differ */
    bool	created;	/* if newly created; for datastore.c (to add .WORDLIST_VERSION) */
    dbe_t	*dbenv;		/* "parent" environment */
    DB_TXN	*txn;		/* transaction in progress or NULL */
} dbh_t;

#define DBT_init(dbt)		(memset(&dbt, 0, sizeof(DBT)))

/* Function definitions */

/** translate BerkeleyDB \a flags bitfield for DB->open method back to symbols */
static const char *resolveopenflags(u_int32_t flags) {
    static char buf[160];
    char b2[80];
    strlcpy(buf, "", sizeof(buf));
    if (flags & DB_CREATE) flags &= ~DB_CREATE, strlcat(buf, "DB_CREATE ", sizeof(buf));
    if (flags & DB_EXCL) flags &= ~DB_EXCL, strlcat(buf, "DB_EXCL ", sizeof(buf));
    if (flags & DB_NOMMAP) flags &= ~DB_NOMMAP, strlcat(buf, "DB_NOMMAP ", sizeof(buf));
    if (flags & DB_RDONLY) flags &= ~DB_RDONLY, strlcat(buf, "DB_RDONLY ", sizeof(buf));
#if DB_AT_LEAST(4,1)
    if (flags & DB_AUTO_COMMIT) flags &= ~DB_AUTO_COMMIT, strlcat(buf, "DB_AUTO_COMMIT ", sizeof(buf));
#endif
    snprintf(b2, sizeof(b2), "%#lx", (unsigned long)flags);
    if (flags) strlcat(buf, b2, sizeof(buf));
    return buf;
}

/** wrapper for Berkeley DB's DB->open() method which has changed API and
 * semantics -- this should deal with 3.2, 3.3, 4.0, 4.1 and 4.2. */
static int DB_OPEN(DB *db, const char *file,
	const char *database, DBTYPE type, u_int32_t flags, int mode)
{
    int ret;

#if DB_AT_LEAST(4,1)
    flags |= DB_AUTO_COMMIT;
#endif

    ret = db->open(db,
#if DB_AT_LEAST(4,1)
	    0,	/* TXN handle - we use autocommit instead */
#endif
	    file, database, type, flags, mode);

    if (DEBUG_DATABASE(1) || getenv("BF_DEBUG_DB_OPEN"))
	fprintf(dbgout, "[pid %lu] DB->open(db=%p, file=%s, database=%s, "
		"type=%x, flags=%#lx=%s, mode=%#o) -> %d %s\n",
		(unsigned long)getpid(), (void *)db, file,
		database ? database : "NIL", type, (unsigned long)flags,
		resolveopenflags(flags), mode, ret, db_strerror(ret));

    return ret;
}

#if DB_AT_LEAST(4,1)
/** translate BerkeleyDB \a flags bitfield for DB->set_flags method back to symbols */
static const char *resolvesetflags(u_int32_t flags) {
    static char buf[160];
    char b2[80];
    strlcpy(buf, "", sizeof(buf));
#if DB_EQUAL(4,1)
    if (flags & DB_CHKSUM_SHA1) flags &= ~DB_CHKSUM_SHA1, strlcat(buf, "DB_CHKSUM_SHA1 ", sizeof(buf));
#endif
#if DB_AT_LEAST(4,2)
    if (flags & DB_CHKSUM) flags &= ~DB_CHKSUM, strlcat(buf, "DB_CHKSUM ", sizeof(buf));
#endif
    snprintf(b2, sizeof(b2), "%#lx", (unsigned long)flags);
    if (flags) strlcat(buf, b2, sizeof(buf));
    return buf;
}

/** Set flags and print debugging info */
static int DB_SET_FLAGS(DB *db, u_int32_t flags)
{
    int ret = db->set_flags(db, flags);

    if (DEBUG_DATABASE(1))
	fprintf(dbgout, "[pid %lu] DB->set_flags(db=%p, flags=%#lx=%s) -> %d %s\n",
		(unsigned long)getpid(), (void *)db, (unsigned long)flags,
		resolvesetflags(flags), ret, db_strerror(ret));

    return ret;
}
#endif

/** "constructor" - allocate our handle and initialize its contents */
static dbh_t *handle_init(const char *path, const char *name)
{
    dbh_t *handle;
    size_t len = strlen(path) + strlen(name) + 2;

    handle = xmalloc(sizeof(dbh_t));
    memset(handle, 0, sizeof(dbh_t));	/* valgrind */

    handle->magic= MAGIC_DBH;		/* poor man's type checking */
    handle->fd   = -1;			/* for lock */

    handle->path = xstrdup(path);
    handle->name = xmalloc(len);
    build_path(handle->name, len, path, name);

    handle->is_swapped = false;
    handle->created    = false;

    return handle;
}

/** free \a handle and associated data.
 * NB: does not close transactions, data bases or the environment! */
static void handle_free(/*@only@*/ dbh_t *handle)
{
    if (handle != NULL) {
	xfree(handle->name);
	xfree(handle->path);
	xfree(handle);
    }
    return;
}

/* Returns is_swapped flag */
bool db_is_swapped(void *vhandle)
{
    dbh_t *handle = vhandle;

    assert(handle->magic == MAGIC_DBH);

    return handle->is_swapped;
}


/* Returns created flag */
bool db_created(void *vhandle)
{
    dbh_t *handle = vhandle;

    assert(handle->magic == MAGIC_DBH);

    return handle->created;
}


/* If header and library version do not match,
 * print an error message on stderr and exit with EX_ERROR. */
static void check_db_version(void)
{
    int maj, min;
    static bool version_ok = false;

    if (!version_ok) {
	version_ok = true;
	(void)db_version(&maj, &min, NULL);

	if (DEBUG_DATABASE(1))
	    fprintf(dbgout, "db_version: Header version %d.%d, library version %d.%d\n",
		    DB_VERSION_MAJOR, DB_VERSION_MINOR, maj, min);

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
#ifndef __EMX__
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
#endif
}

/* The old, pre-3.3 API will not fill in the page size with
 * DB_CACHED_COUNTS, and without DB_CACHED_COUNTS, BerlekeyDB will read
 * the whole data base, incurring a severe performance penalty. We'll
 * guess a page size.  As this is a safety margin for the file size,
 * we'll return 0 and let the caller guess some size instead. */
#if DB_AT_LEAST(3,3)
/* return page size, of 0xffffffff for trouble */
static uint32_t get_psize(DB *dbp)
{
    uint32_t ret, pagesize;
    DB_BTREE_STAT *dbstat = NULL;

    ret = BF_DB_STAT(dbp, NULL, &dbstat, DB_FAST_STAT);
    if (ret) {
	print_error(__FILE__, __LINE__, "DB->stat");
	return 0xffffffff;
    }
    pagesize = dbstat->bt_pagesize;

    if (DEBUG_DATABASE(1))
	fprintf(dbgout, "DB->stat success, pagesize: %lu\n", (unsigned long)pagesize);

    free(dbstat);
    return pagesize;
}
#else
#define get_psize(discard) 0
#endif

const char *db_version_str(void)
{
#ifdef DB_VERSION_STRING
    static const char v[] = DB_VERSION_STRING;
#else
    static char v[80];
    snprintf(v, sizeof(v), "BerkeleyDB (%d.%d.%d)",
	    DB_VERSION_MAJOR, DB_VERSION_MINOR, DB_VERSION_PATCH);
#endif
    return v;
}

/** Initialize database. Expects open environment.
 * \return pointer to database handle on success, NULL otherwise.
 */
void *db_open(void *vhandle, const char *path,
	const char *name, dbmode_t open_mode)
{
    int ret;
    int is_swapped;
    int retries = 2; /* how often do we retry to open after ENOENT+EEXIST
			races? 2 is sufficient unless the kernel or
			BerkeleyDB are buggy. */
    char *t;
    dbe_t *env = vhandle;

    dbh_t *handle = NULL;
    uint32_t opt_flags = (open_mode == DS_READ) ? DB_RDONLY : 0;

    assert(env);
    assert(env->dbe);

    check_db_version();

    {
	DB *dbp;
	uint32_t pagesize;

	handle = handle_init(path, name);

	if (handle == NULL)
	    return NULL;

	/* create DB handle */
	if ((ret = db_create (&dbp, env->dbe, 0)) != 0) {
	    print_error(__FILE__, __LINE__, "(db) db_create, err: %s",
			db_strerror(ret));
	    goto open_err;
	}

	handle->dbp = dbp;
	handle->dbenv = env;

	/* open data base */
	if ((t = strrchr(handle->name, DIRSEP_C)))
	    t++;
	else
	    t = handle->name;

	handle->open_mode = open_mode;

retry_db_open:
	handle->created = false;

	ret = DB_OPEN(dbp, t, NULL, dbtype, opt_flags, 0664);

	if (ret != 0 && ( ret != ENOENT || opt_flags == DB_RDONLY ||
		((handle->created = true),
#if DB_EQUAL(4,1)
		 (ret = DB_SET_FLAGS(dbp, DB_CHKSUM_SHA1)) != 0 ||
#endif
#if DB_AT_LEAST(4,2)
		 (ret = DB_SET_FLAGS(dbp, DB_CHKSUM)) != 0 ||
#endif
		(ret = DB_OPEN(dbp, t, NULL, dbtype, opt_flags | DB_CREATE | DB_EXCL, 0664)) != 0)))
	{
	    if (open_mode != DB_RDONLY && ret == EEXIST && --retries) {
		/* sleep for 4 to 100 ms - this is just to give up the CPU
		 * to another process and let it create the data base
		 * file in peace */
		rand_sleep(4 * 1000, 100 * 1000);
		goto retry_db_open;
	    }

	    /* close again and bail out without further tries */
	    if (DEBUG_DATABASE(0))
		print_error(__FILE__, __LINE__, "DB->open(%s) - actually %s, directory %s, err %s",
			    handle->name, t, env->directory, db_strerror(ret));

	    dbp->close(dbp, 0);
	    goto open_err;
	}

	/* see if the database byte order differs from that of the cpu's */
#if DB_AT_LEAST(3,3)
	ret = dbp->get_byteswapped (dbp, &is_swapped);
#else
	ret = 0;
	is_swapped = dbp->get_byteswapped (dbp);
#endif
	handle->is_swapped = is_swapped ? true : false;

	if (ret != 0) {
	    print_error(__FILE__, __LINE__, "DB->get_byteswapped: %s",
		      db_strerror(ret));
	    db_close(handle);
	    return NULL;		/* handle already freed, ok to return */
	}

	if (DEBUG_DATABASE(1))
	    fprintf(dbgout, "DB->get_byteswapped: %s\n", is_swapped ? "true" : "false");

	ret = dbp->fd(dbp, &handle->fd);
	if (ret != 0) {
	    print_error(__FILE__, __LINE__, "DB->fd: %s",
		      db_strerror(ret));
	    db_close(handle);
	    return NULL;		/* handle already freed, ok to return */
	}

	if (DEBUG_DATABASE(1))
	    fprintf(dbgout, "DB->fd: %d\n", handle->fd);

	/* query page size */
	pagesize = get_psize(dbp);
	if (pagesize == 0xffffffff) {
	    dbp->close(dbp, 0);
	    goto open_err;
	}

	if (!pagesize)
	    pagesize = 16384;

	/* check file size limit */
	check_fsize_limit(handle->fd, pagesize);
    }

    return handle;

 open_err:
    handle_free(handle);

    if (ret >= 0)
	errno = ret;
    else
	errno = EINVAL;
    return NULL;
}

/** begin transaction. Returns 0 for success. */
int db_txn_begin(void *vhandle)
{
    DB_TXN *t;
    int ret;

    dbh_t *dbh = vhandle;
    dbe_t *env;

    assert(dbh);
    assert(dbh->magic == MAGIC_DBH);
    assert(dbh->txn == 0);

    env = dbh->dbenv;

    assert(env);
    assert(env->dbe);

    ret = BF_TXN_BEGIN(env->dbe, NULL, &t, 0);
    if (ret) {
	print_error(__FILE__, __LINE__, "DB_ENV->txn_begin(%p), err: %s",
		(void *)env->dbe, db_strerror(ret));
	return ret;
    }
    dbh->txn = t;

    if (DEBUG_DATABASE(2))
	fprintf(dbgout, "DB_ENV->txn_begin(%p), tid: %lx\n",
		(void *)env->dbe, (unsigned long)BF_TXN_ID(t));

    return 0;
}

int db_txn_abort(void *vhandle)
{
    int ret;
    dbh_t *dbh = vhandle;
    DB_TXN *t;

    assert(dbh);
    assert(dbh->magic == MAGIC_DBH);

    t = dbh->txn;

    assert(t);

    ret = BF_TXN_ABORT(t);
    if (ret)
	print_error(__FILE__, __LINE__, "DB_TXN->abort(%lx) error: %s",
		(unsigned long)BF_TXN_ID(t), db_strerror(ret));
    else
	if (DEBUG_DATABASE(2))
	    fprintf(dbgout, "DB_TXN->abort(%lx)\n",
		    (unsigned long)BF_TXN_ID(t));

    dbh->txn = NULL;

    switch (ret) {
	case 0:
	    return DST_OK;
	case DB_LOCK_DEADLOCK:
	    return DST_TEMPFAIL;
	default:
	    return DST_FAILURE;
    }
}

int db_txn_commit(void *vhandle)
{
    int ret;
    dbh_t *dbh = vhandle;
    DB_TXN *t;
    u_int32_t id;

    assert(dbh);
    assert(dbh->magic == MAGIC_DBH);

    t = dbh->txn;

    assert(t);

    id = BF_TXN_ID(t);
    ret = BF_TXN_COMMIT(t, 0);
    if (ret)
	print_error(__FILE__, __LINE__, "DB_TXN->commit(%lx) error: %s",
		(unsigned long)id, db_strerror(ret));
    else
	if (DEBUG_DATABASE(2))
	    fprintf(dbgout, "DB_TXN->commit(%lx, 0)\n",
		    (unsigned long)id);

    dbh->txn = NULL;

    switch (ret) {
	case 0:
	    /* push out buffer pages so that >=15% are clean - we
	     * can ignore errors here, as the log has all the data */
	    BF_MEMP_TRICKLE(dbh->dbenv->dbe, 15, NULL);

	    return DST_OK;
	case DB_LOCK_DEADLOCK:
	    return DST_TEMPFAIL;
	default:
	    return DST_FAILURE;
    }
}

int db_delete(void *vhandle, const dbv_t *token)
{
    int ret = 0;
    dbh_t *handle = vhandle;
    DB *dbp = handle->dbp;

    DBT db_key;
    DBT_init(db_key);

    assert(handle->magic == MAGIC_DBH);
    assert(handle->txn);

    db_key.data = token->data;
    db_key.size = token->leng;

    ret = dbp->del(dbp, handle->txn, &db_key, 0);

    if (ret != 0 && ret != DB_NOTFOUND) {
	print_error(__FILE__, __LINE__, "DB->del('%.*s'), err: %s",
		    CLAMP_INT_MAX(db_key.size),
		    (const char *) db_key.data,
    		    db_strerror(ret));
	exit(EX_ERROR);
    }

    if (DEBUG_DATABASE(3))
	fprintf(dbgout, "DB->del(%.*s)\n", CLAMP_INT_MAX(db_key.size), (const char *) db_key.data);

    return ret;		/* 0 if ok */
}


int db_get_dbvalue(void *vhandle, const dbv_t *token, /*@out@*/ dbv_t *val)
{
    int ret = 0;
    DBT db_key;
    DBT db_data;

    dbh_t *handle = vhandle;
    DB *dbp = handle->dbp;

    assert(handle);
    assert(handle->magic == MAGIC_DBH);
    assert(handle->txn);

    DBT_init(db_key);
    DBT_init(db_data);

    db_key.data = token->data;
    db_key.size = token->leng;

    db_data.data = val->data;
    db_data.size = val->leng;		/* cur used */
    db_data.ulen = val->leng;		/* max size */
    db_data.flags = DB_DBT_USERMEM;	/* saves the memcpy */

    /* DB_RMW can avoid deadlocks */
    ret = dbp->get(dbp, handle->txn, &db_key, &db_data, handle->open_mode == DS_READ ? 0 : DB_RMW);

    if (DEBUG_DATABASE(3))
	fprintf(dbgout, "DB->get(%.*s): %s\n",
		CLAMP_INT_MAX(token->leng), (char *) token->data, db_strerror(ret));

    val->leng = db_data.size;		/* read count */

    switch (ret) {
    case 0:
	break;
    case DB_NOTFOUND:
	ret = DS_NOTFOUND;
	break;
    case DB_LOCK_DEADLOCK:
	db_txn_abort(handle);
	ret = DS_ABORT_RETRY;
	break;
    default:
	print_error(__FILE__, __LINE__, "(db) DB->get(TXN=%lu,  '%.*s' ), err: %s",
		    (unsigned long)handle->txn, CLAMP_INT_MAX(token->leng),
		    (char *) token->data, db_strerror(ret));
	db_txn_abort(handle);
	exit(EX_ERROR);
    }

    return ret;
}


int db_set_dbvalue(void *vhandle, const dbv_t *token, const dbv_t *val)
{
    int ret;

    DBT db_key;
    DBT db_data;

    dbh_t *handle = vhandle;
    DB *dbp = handle->dbp;

    assert(handle->magic == MAGIC_DBH);
    assert(handle->txn);

    DBT_init(db_key);
    DBT_init(db_data);

    db_key.data = token->data;
    db_key.size = token->leng;

    db_data.data = val->data;
    db_data.size = val->leng;		/* write count */

    ret = dbp->put(dbp, handle->txn, &db_key, &db_data, 0);

    if (ret == DB_LOCK_DEADLOCK) {
	db_txn_abort(handle);
	return DS_ABORT_RETRY;
    }

    if (ret != 0) {
	print_error(__FILE__, __LINE__, "db_set_dbvalue( '%.*s' ), err: %s",
		    CLAMP_INT_MAX(token->leng), (char *)token->data, db_strerror(ret));
	exit(EX_ERROR);
    }

    if (DEBUG_DATABASE(3))
	fprintf(dbgout, "DB->put(%.*s): %s\n",
		CLAMP_INT_MAX(token->leng), (char *) token->data, db_strerror(ret));

    return 0;
}

static int db_flush_dirty(DB_ENV *env, int ret) {
#if DB_AT_LEAST(3,0) && DB_AT_MOST(4,0)
    /* flush dirty pages in buffer pool */
    while (ret == DB_INCOMPLETE) {
	rand_sleep(10000,1000000);
	ret = BF_MEMP_SYNC(env, NULL);
    }
#else
    (void)env;
#endif

    return ret;
}

/* Close files and clean up. */
void db_close(void *vhandle)
{
    int ret;
    dbh_t *handle = vhandle;
    DB *dbp = handle->dbp;
    uint32_t f = DB_NOSYNC;	/* safe as long as we're logging TXNs */
    DB_ENV *dbe = handle->dbenv->dbe;

    assert(handle->magic == MAGIC_DBH);

#if DB_AT_LEAST(4,2)
    {
	/* get_flags and DB_TXN_NOT_DURABLE are new in 4.2 */
	uint32_t t;
	ret = dbp->get_flags(dbp, &t);
	if (ret) {
	    print_error(__FILE__, __LINE__, "DB->get_flags returned error: %s",
		    db_strerror(ret));
	    f = 0;
	} else {
	    if (t & DB_TXN_NOT_DURABLE)
		f &= ~DB_NOSYNC;
	}
    }
#endif

#if DB_AT_LEAST(4,3)
    /* DB_LOG_INMEMORY is new in 4,3 */
    {
	uint32_t t;
	ret = dbe->get_flags(dbe, &t);
	if (ret) {
	    print_error(__FILE__, __LINE__, "DB_ENV->get_flags returned error: %s",
		    db_strerror(ret));
	    f = 0;
	} else {
	    if (t & DB_LOG_INMEMORY)
		f &= ~DB_NOSYNC;
	}
    }
#endif

    if (DEBUG_DATABASE(1))
	fprintf(dbgout, "DB->close(%s, %s)\n",
		handle->name, f & DB_NOSYNC ? "nosync" : "sync");

    if (handle->txn) {
	print_error(__FILE__, __LINE__, "db_close called with transaction still open, program fault!");
    }

    ret = dbp->close(dbp, f);
    ret = db_flush_dirty(dbe, ret);
    if (ret)
	print_error(__FILE__, __LINE__, "DB->close error: %s",
		db_strerror(ret));

    handle->dbp = NULL;
    handle_free(handle);
}


/*
 flush any data in memory to disk
*/
void db_flush(void *vhandle)
{
    int ret;
    dbh_t *handle = vhandle;
    DB *dbp = handle->dbp;

    assert(handle->magic == MAGIC_DBH);

    if (DEBUG_DATABASE(1))
	fprintf(dbgout, "db_flush(%s)\n", handle->name);

    ret = dbp->sync(dbp, 0);
    ret = db_flush_dirty(handle->dbenv->dbe, ret);

    if (DEBUG_DATABASE(1))
	fprintf(dbgout, "DB->sync(%p): %s\n", (void *)dbp, db_strerror(ret));

    if (ret)
	print_error(__FILE__, __LINE__, "db_sync: err: %s", db_strerror(ret));

    ret = BF_LOG_FLUSH(handle->dbenv->dbe, NULL);
    if (DEBUG_DATABASE(1))
	fprintf(dbgout, "DB_ENV->log_flush(%p): %s\n", (void *)handle->dbenv->dbe,
		db_strerror(ret));
}

ex_t db_foreach(void *vhandle, db_foreach_t hook, void *userdata)
{
    dbh_t *handle = vhandle;
    DB *dbp = handle->dbp;

    ex_t ret = EX_OK;
    bool eflag = false;

    DBC dbc;
    DBC *dbcp = &dbc;
    DBT key, data;

    dbv_t dbv_key, dbv_data;

    assert(handle->magic == MAGIC_DBH);
    assert(handle->dbenv->dbe);
    assert(handle->txn);

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    ret = dbp->cursor(dbp, handle->txn, &dbcp, 0);
    if (ret) {
	print_error(__FILE__, __LINE__, "(cursor): %s", handle->path);
	return EX_ERROR;
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
	ret = EX_OK;
	break;
    default:
	print_error(__FILE__, __LINE__, "(c_get): %s", db_strerror(ret));
	eflag = true;
	break;
    }

    if ((ret = dbcp->c_close(dbcp))) {
	print_error(__FILE__, __LINE__, "(c_close): %s", db_strerror(ret));
	eflag = true;
    }

    return eflag ? EX_ERROR : ret;
}

const char *db_str_err(int e) {
    return db_strerror(e);
}

/** set an fcntl-style lock on \a path.
 * \a locktype is F_RDLCK, F_WRLCK, F_UNLCK
 * \a mode is F_SETLK or F_SETLKW
 * \return file descriptor of locked file if successful
 * negative value in case of error
 */
static int plock(const char *path, short locktype, int mode) {
    struct flock fl;
    int fd, r;

    fd = open(path, O_RDWR);
    if (fd < 0) return fd;

    fl.l_type = locktype;
    fl.l_whence = SEEK_SET;
    fl.l_start = (off_t)0;
    fl.l_len = (off_t)0;
    r = fcntl(fd, mode, &fl);
    if (r < 0)
	return r;
    return fd;
}

static int db_try_glock(const char *directory, short locktype, int lockcmd) {
    int ret;
    char *t;
    const char *const tackon = DIRSEP_S "lockfile-d";

    assert(directory);

    /* lock */
    ret = mkdir(directory, (mode_t)0755);
    if (ret && errno != EEXIST) {
	print_error(__FILE__, __LINE__, "mkdir(%s): %s",
		directory, strerror(errno));
	exit(EX_ERROR);
    }

    t = mxcat(directory, tackon, NULL);

    /* All we are interested in is that this file exists, we'll close it
     * right away as plock down will open it again */
    ret = open(t, O_RDWR|O_CREAT|O_EXCL, (mode_t)0664);
    if (ret < 0 && errno != EEXIST) {
	print_error(__FILE__, __LINE__, "open(%s): %s",
		t, strerror(errno));
	exit(EX_ERROR);
    }

    if (ret >= 0)
	close(ret);

    lockfd = plock(t, locktype, lockcmd);
    if (lockfd < 0 && errno != EAGAIN && errno != EACCES) {
	print_error(__FILE__, __LINE__, "lock(%s): %s",
		t, strerror(errno));
	exit(EX_ERROR);
    }

    free(t);
    /* lock set up */
    return lockfd;
}

/** Create environment or exit with EX_ERROR */
static int bf_dbenv_create(DB_ENV **env)
{
    int ret = db_env_create(env, 0);
    if (ret != 0) {
	print_error(__FILE__, __LINE__, "db_env_create, err: %s",
		db_strerror(ret));
	exit(EX_ERROR);
    }
    if (DEBUG_DATABASE(1))
	fprintf(dbgout, "db_env_create: %p\n", (void *)env);
    (*env)->set_errfile(*env, stderr);

    return ret;
}


/* dummy infrastructure, to be expanded by environment
 * or transactional initialization/shutdown */
static dbe_t *dbe_xinit(const char *directory, u_int32_t numlocks, u_int32_t numobjs, u_int32_t flags)
{
    int ret;
    u_int32_t logsize = 1048576;    /* 1 MByte (default in BDB 10 MByte) */
    dbe_t *env = xcalloc(1, sizeof(dbe_t));

    assert(directory);

    env->magic = MAGIC_DBE;	    /* poor man's type checking */
    env->directory = xstrdup(directory);
    ret = bf_dbenv_create(&env->dbe);

    if (db_cachesize != 0 &&
	    (ret = env->dbe->set_cachesize(env->dbe, db_cachesize/1024, (db_cachesize % 1024) * 1024*1024, 1)) != 0) {
	print_error(__FILE__, __LINE__, "DB_ENV->set_cachesize(%u), err: %s",
		db_cachesize, db_strerror(ret));
	exit(EX_ERROR);
    }

    if (DEBUG_DATABASE(1))
	fprintf(dbgout, "DB_ENV->set_cachesize(%u)\n", db_cachesize);

    /* configure lock system size - locks */
#if DB_AT_LEAST(3,2)
    if ((ret = env->dbe->set_lk_max_locks(env->dbe, numlocks)) != 0)
#else
    if ((ret = env->dbe->set_lk_max(env->dbe, numlocks)) != 0)
#endif
    {
	print_error(__FILE__, __LINE__, "DB_ENV->set_lk_max_locks(%p, %lu), err: %s", (void *)env->dbe,
		(unsigned long)numlocks, db_strerror(ret));
	exit(EX_ERROR);
    }

    if (DEBUG_DATABASE(1))
	fprintf(dbgout, "DB_ENV->set_lk_max_locks(%p, %lu)\n", (void *)env->dbe, (unsigned long)numlocks);

#if DB_AT_LEAST(3,2)
    /* configure lock system size - objects */
    if ((ret = env->dbe->set_lk_max_objects(env->dbe, numobjs)) != 0) {
	print_error(__FILE__, __LINE__, "DB_ENV->set_lk_max_objects(%p, %lu), err: %s", (void *)env->dbe,
		(unsigned long)numobjs, db_strerror(ret));
	exit(EX_ERROR);
    }
    if (DEBUG_DATABASE(1))
	fprintf(dbgout, "DB_ENV->set_lk_max_objects(%p, %lu)\n", (void *)env->dbe, (unsigned long)numlocks);
#else
    /* suppress compiler warning for unused variable */
    (void)numobjs;
#endif

    /* configure automatic deadlock detector */
    if ((ret = env->dbe->set_lk_detect(env->dbe, DB_LOCK_DEFAULT)) != 0) {
	print_error(__FILE__, __LINE__, "DB_ENV->set_lk_detect(DB_LOCK_DEFAULT), err: %s", db_strerror(ret));
	exit(EX_ERROR);
    }

    if (DEBUG_DATABASE(1))
	fprintf(dbgout, "DB_ENV->set_lk_detect(DB_LOCK_DEFAULT)\n");

    /* configure log file size */
    ret = env->dbe->set_lg_max(env->dbe, logsize);
    if (ret) {
	print_error(__FILE__, __LINE__, "DB_ENV->set_lg_max(%lu) err: %s",
		(unsigned long)logsize, db_strerror(ret));
	exit(EX_ERROR);
    }

    if (DEBUG_DATABASE(1))
	fprintf(dbgout, "DB_ENV->set_lg_max(%lu)\n", (unsigned long)logsize);

    ret = env->dbe->open(env->dbe, directory,
	    dbenv_defflags | DB_CREATE | flags, /* mode */ 0664);
    if (ret != 0) {
	env->dbe->close(env->dbe, 0);
	print_error(__FILE__, __LINE__, "DB_ENV->open, err: %s", db_strerror(ret));
	switch (ret) {
	    case DB_RUNRECOVERY:
		fprintf(stderr, "To recover, run: bogoutil -v --db-recover \"%s\"\n",
			directory);
		break;
	    case EINVAL:
		fprintf(stderr, "\n"
			"If you have just got a message that only private environments are supported,\n"
			"your Berkeley DB %d.%d was not configured properly.\n"
			"Bogofilter requires shared environments to support Berkeley DB transactions.\n",
			DB_VERSION_MAJOR, DB_VERSION_MINOR);
		fprintf(stderr,
			"Reconfigure and recompile Berkeley DB with the right mutex interface,\n"
			"see the docs/ref/build_unix/conf.html file that comes with your db source code.\n"
			"This can happen when the DB library was compiled with POSIX threads\n"
			"but your system does not support NPTL.\n");
		break;
	}

	exit(EX_ERROR);
    }

    if (DEBUG_DATABASE(1))
	fprintf(dbgout, "DB_ENV->open(home=%s)\n", directory);

    return env;
}

/* close the environment, but do not release locks */
static void dbe_cleanup_lite(dbe_t *env) {
    if (env->dbe) {
	int ret;

	/* checkpoint if more than 64 kB of logs have been written
	 * or 120 min have passed since the previous checkpoint */
	/*                                kB  min flags */
	ret = BF_TXN_CHECKPOINT(env->dbe, 64, 120, 0);
	ret = db_flush_dirty(env->dbe, ret);
	if (ret)
	    print_error(__FILE__, __LINE__, "DBE->txn_checkpoint returned %s", db_strerror(ret));

	ret = env->dbe->close(env->dbe, 0);
	if (DEBUG_DATABASE(1) || ret)
	    fprintf(dbgout, "DB_ENV->close(%p): %s\n", (void *)env->dbe, db_strerror(ret));
    }
    if (env->directory)
	free(env->directory);
    free(env);
}

/* initialize data base, configure some lock table sizes
 * (which can be overridden in the DB_CONFIG file)
 * and lock the file to tell other parts we're initialized and
 * do not want recovery to stomp over us
 */
void *dbe_init(const char *directory) {
    u_int32_t flags = 0;
    char norm_dir[PATH_MAX+1]; /* check normalized directory names */
    char norm_home[PATH_MAX+1];/* see man realpath(3) for details */

    if (NULL == realpath(directory, norm_dir)) {
	    print_error(__FILE__, __LINE__,
		    "error: cannot normalize path \"%s\": %s",
		    directory, strerror(errno));
	    exit(EX_ERROR);
    }

    if (NULL == realpath(bogohome, norm_home)) {
	    print_error(__FILE__, __LINE__,
		    "error: cannot normalize path \"%s\": %s",
		    bogohome, strerror(errno));
	    exit(EX_ERROR);
    }

    if (0 != strcmp(norm_dir, norm_home))
    {
	fprintf(stderr,
		"ERROR: only one database _environment_ (directory) can be used at a time.\n"
		"You CAN use multiple wordlists that are in the same directory.\n\n");
	fprintf(stderr,
		"If you need multiple wordlists in different directories,\n"
		"you cannot use the transactional interface, but you must configure\n"
		"the non-transactional interface, i. e. ./configure --disable-transactions\n"
		"then type make clean, after that rebuild and install as usual.\n"
		"Note that the data base will no longer be crash-proof in that case.\n"
		"Please accept our apologies for the inconvenience.\n");
	fprintf(stderr,
		"\nAborting program\n");
	exit(EX_ERROR);
    }

    /* open lock file, needed to detect previous crashes */
    if (init_dbl(directory))
	exit(EX_ERROR);

    /* run recovery if needed */
    if (needs_recovery())
	dbe_recover(directory, false, false); /* DO NOT set force flag here, may cause
						 multiple recovery! */

    /* set (or demote to) shared/read lock for regular operation */
    db_try_glock(directory, F_RDLCK, F_SETLKW);

    /* set our cell lock in the crash detector */
    if (set_lock()) {
	exit(EX_ERROR);
    }

    /* initialize */

#ifdef	FUTURE_DB_OPTIONS
#ifdef	DB_LOG_AUTOREMOVE
    if (db_log_autoremove)
	flags ^= DB_LOG_AUTOREMOVE;
#endif

#ifdef	DB_TXN_NOT_DURABLE
    if (db_txn_durable)
	flags ^= DB_TXN_NOT_DURABLE;
#endif
#endif

    return dbe_xinit(directory, db_max_locks, db_max_objects, flags);
}

ex_t dbe_recover(const char *directory, bool catastrophic, bool force) {
    dbe_t *env;

    /* set exclusive/write lock for recovery */
    while((force || needs_recovery())
	    && (db_try_glock(directory, F_WRLCK, F_SETLKW) <= 0))
	rand_sleep(10000,1000000);

    /* ok, when we have the lock, a concurrent process may have
     * proceeded with recovery */
    if (!(force || needs_recovery()))
	return EX_OK;

retry:
    if (DEBUG_DATABASE(0))
        fprintf(dbgout, "running %s data base recovery\n",
	    catastrophic ? "catastrophic" : "regular");
    env = dbe_xinit(directory, 1024, 1024, catastrophic ? DB_RECOVER_FATAL : DB_RECOVER);
    if (env == NULL) {
	if(!catastrophic) {
	    catastrophic = true;
	    goto retry;
	}
	goto rec_fail;
    }

    clear_lockfile();
    dbe_cleanup_lite(env);

    return EX_OK;

rec_fail:
    exit(EX_ERROR);
}

void dbe_cleanup(void *vhandle) {
    dbe_t *env = vhandle;

    assert(env->magic == MAGIC_DBE);

    dbe_cleanup_lite(env);
    clear_lock();
    if (lockfd >= 0)
	close(lockfd); /* release locks */
}

void *db_get_env(void *vhandle) {
    dbh_t *handle = vhandle;

    assert(handle->magic == MAGIC_DBH);

    return handle->dbenv;
}

static DB_ENV *dbe_recover_open(const char *directory, uint32_t flags) {
    const uint32_t local_flags = flags | DB_CREATE;
    DB_ENV *env;
    int e;

    if (DEBUG_DATABASE(0))
        fprintf(dbgout, "trying to lock database directory\n");
    db_try_glock(directory, F_WRLCK, F_SETLKW); /* wait for exclusive lock */

    /* run recovery */
    bf_dbenv_create(&env);

    if (DEBUG_DATABASE(0))
        fprintf(dbgout, "running regular data base recovery%s\n",
	       flags & DB_PRIVATE ? " and removing environment" : "");

    /* quirk: DB_RECOVER requires DB_CREATE and cannot work with DB_JOINENV */

    /*
     * Hint from Keith Bostic, SleepyCat support, 2004-11-29,
     * we can use the DB_PRIVATE flag, that rebuilds the database
     * environment in heap memory, so we don't need to remove it.
     */

    e = env->open(env, directory,
	    dbenv_defflags | local_flags | DB_RECOVER, 0664);
    if (e == DB_RUNRECOVERY) {
	/* that didn't work, try harder */
	if (DEBUG_DATABASE(0))
	    fprintf(dbgout, "running catastrophic data base recovery\n");
	e = env->open(env, directory,
		dbenv_defflags | local_flags | DB_RECOVER_FATAL, 0664);
    }
    if (e) {
	print_error(__FILE__, __LINE__, "Cannot recover environment \"%s\": %s",
		directory, db_strerror(e));
	exit(EX_ERROR);
    }

    return env;
}

static ex_t dbe_common_close(DB_ENV *env, const char *directory) {
    int e;

    e = env->close(env, 0);
    if (e != 0) {
	print_error(__FILE__, __LINE__, "Error closing environment \"%s\": %s",
		directory, db_strerror(e));
	exit(EX_ERROR);
    }

    db_try_glock(directory, F_UNLCK, F_SETLKW); /* release lock */
    return EX_OK;
}

ex_t dbe_purgelogs(const char *directory) {
    int e;
    DB_ENV *env = dbe_recover_open(directory, 0);
    char **i, **list;

    if (!env)
	exit(EX_ERROR);

    if (DEBUG_DATABASE(0))
	fprintf(dbgout, "checkpoint database\n");

    /* checkpoint the transactional system */
    e = BF_TXN_CHECKPOINT(env, 0, 0, 0);
    e = db_flush_dirty(env, e);
    if (e) {
	print_error(__FILE__, __LINE__, "DB_ENV->txn_checkpoint failed: %s",
		db_strerror(e));
	exit(EX_ERROR);
    }

    if (DEBUG_DATABASE(0))
	fprintf(dbgout, "removing inactive logfiles\n");

    /* figure redundant log files and nuke them */
    e = BF_LOG_ARCHIVE(env, &list, DB_ARCH_ABS);
    if (e) {
	print_error(__FILE__, __LINE__,
		"DB_ENV->log_archive failed: %s",
		db_strerror(e));
	exit(EX_ERROR);
    }

    if (list != NULL) {
	for (i = list; *i != NULL; i++) {
	    if (DEBUG_DATABASE(1))
		fprintf(dbgout, " removing logfile %s\n", *i);
	    if (unlink(*i)) {
		print_error(__FILE__, __LINE__,
			"cannot unlink \"%s\": %s", *i, strerror(errno));
		/* proceed anyways */
	    }
	}
	free(list);
    }

    if (DEBUG_DATABASE(0))
	fprintf(dbgout, "closing environment\n");

    return dbe_common_close(env, directory);
}

ex_t db_verify(const char *dbfile) {
    char *dir;
    char *tmp;
    DB_ENV *env;
    DB *db;
    int e;

    if (!is_file(dbfile)) {
	print_error(__FILE__, __LINE__, "\"%s\" is not a file.", dbfile);
	return EX_ERROR;
    }

    dir = xstrdup(dbfile);
    tmp = strrchr(dir, DIRSEP_C);
    if (!tmp)
	free(dir), dir = xstrdup(CURDIR_S);
    else
	*tmp = '\0';

    env = dbe_recover_open(dir, 0); /* this sets an exclusive lock */
    e = db_create(&db, NULL, 0); /* do not use environment here, verify
				    does not lock by itself, we hold the
				    global lock instead! */
    if (e != 0) {
	print_error(__FILE__, __LINE__, "error creating DB handle: %s",
		db_strerror(e));
	free(dir);
	exit(EX_ERROR);
    }
    e = db->verify(db, dbfile, NULL, NULL, 0);
    if (e) {
	print_error(__FILE__, __LINE__, "database %s does not verify: %s",
		dbfile, db_strerror(e));
	free(dir);
	exit(EX_ERROR);
    }
    e = dbe_common_close(env, dir);
    free(dir);
    if (e == 0 && verbose)
	printf("%s OK.\n", dbfile);
    return e;
}

ex_t dbe_remove(const char *directory) {
    DB_ENV *env = dbe_recover_open(directory, DB_PRIVATE);

    if (!env)
	exit(EX_ERROR);

    return dbe_common_close(env, directory);
}
