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

/* This code has been tested with BerkeleyDB 3.0, 3.1 3.2, 3.3, 4.0,
 * 4.1 and 4.2.  -- Matthias Andree, 2004-10-04 */

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

static DB_ENV *dbe;	/* libdb environment, if in use, NULL otherwise */
static int lockfd = -1;	/* fd of lock file to prevent concurrent recovery */

static const DBTYPE dbtype = DB_BTREE;

static bool init = false;

typedef struct {
    char	*path;
    char	*name;
    int		fd;		/* file descriptor of data base file */
    dbmode_t	open_mode;	/* datastore open mode, DS_READ/DS_WRITE */
    DB		*dbp;		/* data base handle */
    bool	is_swapped;	/* set if CPU and data base endianness differ */
    DB_TXN	*txn;		/* stores the transaction handle */
    bool	created;	/* if newly created; for datastore.c (to add .WORDLIST_VERSION) */
} dbh_t;

#define DBT_init(dbt)		(memset(&dbt, 0, sizeof(DBT)))

#define DB_AT_LEAST(maj, min)	((DB_VERSION_MAJOR > (maj)) || ((DB_VERSION_MAJOR == (maj)) && (DB_VERSION_MINOR >= (min))))
#define DB_AT_MOST(maj, min)	((DB_VERSION_MAJOR < (maj)) || ((DB_VERSION_MAJOR == (maj)) && (DB_VERSION_MINOR <= (min))))
#define DB_EQUAL(maj, min)	((DB_VERSION_MAJOR == (maj)) && (DB_VERSION_MINOR == (min)))

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
#if DB_EQUAL(4,2)
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
static dbh_t *dbh_init(const char *path, const char *name)
{
    dbh_t *handle;
    size_t len = strlen(path) + strlen(name) + 2;

    handle = xmalloc(sizeof(dbh_t));
    memset(handle, 0, sizeof(dbh_t));	/* valgrind */

    handle->fd   = -1;

    handle->path = xstrdup(path);
    handle->name = xmalloc(len);
    build_path(handle->name, len, path, name);

    handle->is_swapped = false;
    handle->created    = false;

    handle->txn = NULL;

    return handle;
}

/** free \a handle and associated data.
 * NB: does not close transactions, data bases or the environment! */
static void dbh_free(/*@only@*/ dbh_t *handle)
{
    if (handle != NULL) {
	xfree(handle->name);
	xfree(handle->path);
	xfree(handle);
    }
    return;
}

/* If header and library version do not match,
 * print an error message on stderr and exit with EX_ERROR. */

/* Returns is_swapped flag */
bool db_is_swapped(void *vhandle)
{
    dbh_t *handle = vhandle;
    return handle->is_swapped;
}


/* Returns created flag */
bool db_created(void *vhandle)
{
    dbh_t *handle = vhandle;
    return handle->created;
}


/* If header and library version do not match,
 * print an error message on stderr and exit with EX_ERROR. */
static void check_db_version(void)
{
    int maj, min;
    static int version_ok;

    if (!version_ok) {
	version_ok = 1;
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

    ret = dbp->stat(dbp, &dbstat, DB_FAST_STAT);
    if (ret) {
	print_error(__FILE__, __LINE__, "(db) DB->stat");
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
    static char v[80];
    snprintf(v, sizeof(v), "BerkeleyDB (%d.%d.%d)",
	    DB_VERSION_MAJOR, DB_VERSION_MINOR, DB_VERSION_PATCH);
    return v;
}

/** Initialize database. Expects open environment.
 * \return pointer to database handle on success, NULL otherwise.
 */
void *db_open(const char *path, const char *name, dbmode_t open_mode)
{
    int ret;
    int is_swapped;
    int retries = 2; /* how often do we retry to open after ENOENT+EEXIST
			races? 2 is sufficient unless the kernel or
			BerkeleyDB are buggy. */
    char *t;

    dbh_t *handle = NULL;
    uint32_t opt_flags = 0;

    assert(init);

    check_db_version();

    {
	DB *dbp;
	uint32_t pagesize;

	handle = dbh_init(path, name);

	if (handle == NULL)
	    return NULL;

	/* create DB handle */
	if ((ret = db_create (&dbp, dbe, 0)) != 0) {
	    print_error(__FILE__, __LINE__, "(db) db_create, err: %d, %s",
			ret, db_strerror(ret));
	    goto open_err;
	}

	handle->dbp = dbp;

	/* open data base */
	if ((t = strrchr(handle->name, DIRSEP_C)))
	    t++;
	else
	    t = handle->name;

retry_db_open:
	handle->created = false;
	if ((ret = DB_OPEN(dbp, t, NULL, dbtype, opt_flags, 0664)) != 0
	    && ( ret != ENOENT || opt_flags == DB_RDONLY ||
		((handle->created = true),
#if DB_EQUAL(4,1)
		 (ret = DB_SET_FLAGS(dbp, DB_CHKSUM_SHA1)) != 0 ||
#endif
#if DB_EQUAL(4,2)
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
		print_error(__FILE__, __LINE__, "(db) DB->open(%s) - actually %s, bogohome %s, err %d, %s",
			    handle->name, t, bogohome, ret, db_strerror(ret));
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
	    print_error(__FILE__, __LINE__, "(db) DB->get_byteswapped: %s",
		      db_strerror(ret));
	    db_close(handle);
	    return NULL;		/* handle already freed, ok to return */
	}

	if (DEBUG_DATABASE(1))
	    fprintf(dbgout, "DB->get_byteswapped: %s\n", is_swapped ? "true" : "false");

	ret = dbp->fd(dbp, &handle->fd);
	if (ret != 0) {
	    print_error(__FILE__, __LINE__, "(db) DB->fd: %s",
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
    dbh_free(handle);

    if (ret >= 0)
	errno = ret;
    else
	errno = EINVAL;
    return NULL;
}

/* wrapper for the API that changed in 4.0, to
 * collect the junk in a location separate from the implementation */
#if DB_AT_LEAST(4,0)
/* BerkeleyDB 4.0, 4.1, 4.2 */
#define BF_LOG_FLUSH(e, i) ((e)->log_flush((e), (i)))
#define BF_MEMP_SYNC(e, l) ((e)->memp_sync((e), (l)))
#define BF_MEMP_TRICKLE(e, p, n) ((e)->memp_trickle((e), (p), (n)))
#define BF_TXN_BEGIN(e, f, g, h) ((e)->txn_begin((e), (f), (g), (h)))
#define BF_TXN_ID(t) ((t)->id(t))
#define BF_TXN_ABORT(t) ((t)->abort((t)))
#define BF_TXN_COMMIT(t, f) ((t)->commit((t), (f)))
#define BF_TXN_CHECKPOINT(e, k, m, f) ((e)->txn_checkpoint((e), (k), (m), (f)))
#else
/* BerkeleyDB 3.0, 3.1, 3.2, 3.3 */
#define BF_LOG_FLUSH(e, i) (log_flush((e), (i)))
#define BF_MEMP_SYNC(e, l) (memp_sync((e), (l)))
#define BF_MEMP_TRICKLE(e, p, n) (memp_trickle((e), (p), (n)))
#define BF_TXN_BEGIN(e, f, g, h) (txn_begin((e), (f), (g), (h)))
#define BF_TXN_ID(t) (txn_id(t))
#define BF_TXN_ABORT(t) (txn_abort((t)))
#define BF_TXN_COMMIT(t, f) (txn_commit((t), (f)))
#if DB_AT_LEAST(3,1)
/* BerkeleyDB 3.1, 3.2, 3.3 */
#define BF_TXN_CHECKPOINT(e, k, m, f) (txn_checkpoint((e), (k), (m), (f)))
#else
/* BerkeleyDB 3.0 */
#define BF_TXN_CHECKPOINT(e, k, m, f) (txn_checkpoint((e), (k), (m)))
#endif
#endif

/** begin transaction. Returns 0 for success. */
int db_txn_begin(void *vhandle)
{
    DB_TXN *t;
    int ret;

    dbh_t *handle = vhandle;
    assert(dbe);
    assert(handle);

    ret = BF_TXN_BEGIN(dbe, NULL, &t, 0);
    if (ret) {
	print_error(__FILE__, __LINE__, "DB_ENV->txn_begin(%p), err: %s",
		(void *)dbe, db_strerror(ret));
	return ret;
    }
    handle->txn = t;
    if (DEBUG_DATABASE(2))
	fprintf(dbgout, "DB_ENV->txn_begin(%p), tid: %lx\n",
		(void *)dbe, (unsigned long)BF_TXN_ID(t));

    return 0;
}

int db_txn_abort(void *vhandle)
{
    int ret;
    dbh_t *handle = vhandle;
    DB_TXN *t = handle->txn;
    assert(dbe);
    assert(t);

    ret = BF_TXN_ABORT(t);
    if (ret)
	print_error(__FILE__, __LINE__, "DB_TXN->abort(%lx) error: %s",
		(unsigned long)BF_TXN_ID(t), db_strerror(ret));
    else
	if (DEBUG_DATABASE(2))
	    fprintf(dbgout, "DB_TXN->abort(%lx)\n",
		    (unsigned long)BF_TXN_ID(t));
    handle->txn = NULL;

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
    dbh_t *handle = vhandle;
    DB_TXN *t = handle->txn;
    u_int32_t id;
    assert(dbe);
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
    handle->txn = NULL;

    switch (ret) {
	case 0:
	    /* push out buffer pages so that >=15% are clean - we
	     * can ignore errors here, as the log has all the data */
	    BF_MEMP_TRICKLE(dbe, 15, NULL);

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

    assert(handle->txn);

    db_key.data = token->data;
    db_key.size = token->leng;

    ret = dbp->del(dbp, handle->txn, &db_key, 0);

    if (ret != 0 && ret != DB_NOTFOUND) {
	print_error(__FILE__, __LINE__, "(db) DB->del('%.*s'), err: %d, %s",
		    CLAMP_INT_MAX(db_key.size),
		    (const char *) db_key.data,
    		    ret, db_strerror(ret));
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
	print_error(__FILE__, __LINE__, "(db) DB->get(TXN=%lu,  '%.*s' ), err: %d, %s",
		    (unsigned long)handle->txn, CLAMP_INT_MAX(token->leng), (char *) token->data, ret, db_strerror(ret));
	db_txn_abort(handle);
	exit(EX_ERROR);
    }

    val->leng = db_data.size;		/* read count */

    return ret;
}


int db_set_dbvalue(void *vhandle, const dbv_t *token, dbv_t *val)
{
    int ret;

    DBT db_key;
    DBT db_data;

    dbh_t *handle = vhandle;
    DB *dbp = handle->dbp;
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
	print_error(__FILE__, __LINE__, "(db) db_set_dbvalue( '%.*s' ), err: %d, %s",
		    CLAMP_INT_MAX(token->leng), (char *)token->data, ret, db_strerror(ret));
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
    uint32_t f = DB_NOSYNC; /* safe as long as we're logging TXNs*/

#if DB_AT_LEAST(4,2)
    /* get_flags and DB_TXN_NOT_DURABLE are new in 4.2 */
    ret = dbe->get_flags(dbe, &f);
    if (ret) {
	print_error(__FILE__, __LINE__, "get_flags returned error: %s",
		db_strerror(ret));
	f = 0;
    } else {
	f = (f & DB_TXN_NOT_DURABLE) ? 0 : DB_NOSYNC;
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
	print_error(__FILE__, __LINE__, "(db) db_close err: %d, %s", ret, db_strerror(ret));

    handle->dbp = NULL;
    dbh_free(handle);
}


/*
 flush any data in memory to disk
*/
void db_flush(void *vhandle)
{
    int ret;
    dbh_t *handle = vhandle;
    DB *dbp = handle->dbp;

    if (DEBUG_DATABASE(1))
	fprintf(dbgout, "db_flush(%s)\n", handle->name);

    ret = dbp->sync(dbp, 0);
    ret = db_flush_dirty(dbe, ret);

    if (DEBUG_DATABASE(1))
	fprintf(dbgout, "DB->sync(%p): %s\n", (void *)dbp, db_strerror(ret));

    if (ret)
	print_error(__FILE__, __LINE__, "(db) db_sync: err: %d, %s", ret, db_strerror(ret));

    ret = BF_LOG_FLUSH(dbe, NULL);
    if (DEBUG_DATABASE(1))
	fprintf(dbgout, "DB_ENV->log_flush(%p): %s\n", (void *)dbe,
		db_strerror(ret));
}

int db_foreach(void *vhandle, db_foreach_t hook, void *userdata)
{
    dbh_t *handle = vhandle;
    DB *dbp = handle->dbp;

    int ret = 0, eflag = 0;

    DBC dbc;
    DBC *dbcp = &dbc;
    DBT key, data;

    dbv_t dbv_key, dbv_data;
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    assert(dbe);
    assert(handle->txn);

    ret = dbp->cursor(dbp, handle->txn, &dbcp, 0);
    if (ret) {
	print_error(__FILE__, __LINE__, "(cursor): %s", handle->path);
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
	print_error(__FILE__, __LINE__, "(c_get): %s", db_strerror(ret));
	eflag = 1;
	break;
    }

    if ((ret = dbcp->c_close(dbcp))) {
	print_error(__FILE__, __LINE__, "(c_close): %s", db_strerror(ret));
	eflag = -1;
    }

    return eflag ? -1 : ret;		/* 0 if ok */
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

static int db_try_glock(short locktype, int lockcmd) {
    int ret;
    char *t;
    const char *const tackon = DIRSEP_S "lockfile-d";

    assert(bogohome);

    /* lock */
    ret = mkdir(bogohome, (mode_t)0755);
    if (ret && errno != EEXIST) {
	print_error(__FILE__, __LINE__, "mkdir(%s): %s",
		bogohome, strerror(errno));
	exit(EXIT_FAILURE);
    }

    t = mxcat(bogohome, tackon, NULL);

    /* All we are interested in is that this file exists, we'll close it
     * right away as plock down will open it again */
    ret = open(t, O_RDWR|O_CREAT|O_EXCL, (mode_t)0644);
    if (ret < 0 && errno != EEXIST) {
	print_error(__FILE__, __LINE__, "open(%s): %s",
		t, strerror(errno));
	exit(EXIT_FAILURE);
    }

    if (ret >= 0)
	close(ret);

    lockfd = plock(t, locktype, lockcmd);
    if (lockfd < 0 && errno != EAGAIN && errno != EACCES) {
	print_error(__FILE__, __LINE__, "lock(%s): %s",
		t, strerror(errno));
	exit(EXIT_FAILURE);
    }

    free(t);
    /* lock set up */
    return lockfd;
}

/* dummy infrastructure, to be expanded by environment
 * or transactional initialization/shutdown */
static int db_xinit(u_int32_t numlocks, u_int32_t numobjs, u_int32_t flags)
{
    int ret;

    assert(bogohome);
    assert(dbe == NULL);

    ret = db_env_create(&dbe, 0);
    if (ret != 0) {
	print_error(__FILE__, __LINE__, "db_env_create, err: %d, %s", ret,
		db_strerror(ret));
	exit(EX_ERROR);
    }
    if (DEBUG_DATABASE(1))
	fprintf(dbgout, "db_env_create: %p\n", (void *)dbe);

    dbe->set_errfile(dbe, stderr);

    if (db_cachesize != 0 &&
	(ret = dbe->set_cachesize(dbe, db_cachesize/1024, (db_cachesize % 1024) * 1024*1024, 1)) != 0) {
      print_error(__FILE__, __LINE__, "DB_ENV->set_cachesize(%u), err: %d, %s",
		  db_cachesize, ret, db_strerror(ret));
      exit(EXIT_FAILURE);
    }

    if (DEBUG_DATABASE(1))
	fprintf(dbgout, "DB_ENV->set_cachesize(%u)\n", db_cachesize);

    /* configure lock system size - locks */
#if DB_AT_LEAST(3,2)
    if ((ret = dbe->set_lk_max_locks(dbe, numlocks)) != 0)
#else
    if ((ret = dbe->set_lk_max(dbe, numlocks)) != 0)
#endif
    {
	print_error(__FILE__, __LINE__, "DB_ENV->set_lk_max_locks(%p, %lu), err: %s", (void *)dbe,
		(unsigned long)numlocks, db_strerror(ret));
	exit(EXIT_FAILURE);
    }
    if (DEBUG_DATABASE(1))
	fprintf(dbgout, "DB_ENV->set_lk_max_locks(%p, %lu)\n", (void *)dbe, (unsigned long)numlocks);

#if DB_AT_LEAST(3,2)
    /* configure lock system size - objects */
    if ((ret = dbe->set_lk_max_objects(dbe, numobjs)) != 0) {
	print_error(__FILE__, __LINE__, "DB_ENV->set_lk_max_objects(%p, %lu), err: %s", (void *)dbe,
		(unsigned long)numobjs, db_strerror(ret));
	exit(EXIT_FAILURE);
    }
    if (DEBUG_DATABASE(1))
	fprintf(dbgout, "DB_ENV->set_lk_max_objects(%p, %lu)\n", (void *)dbe, (unsigned long)numlocks);
#endif

    /* configure automatic deadlock detector */
    if ((ret = dbe->set_lk_detect(dbe, DB_LOCK_DEFAULT)) != 0) {
	print_error(__FILE__, __LINE__, "DB_ENV->set_lk_detect(DB_LOCK_DEFAULT), err: %s", db_strerror(ret));
	exit(EXIT_FAILURE);
    }
    if (DEBUG_DATABASE(1))
	fprintf(dbgout, "DB_ENV->set_lk_detect(DB_LOCK_DEFAULT)\n");

    ret = dbe->open(dbe, bogohome,
#if DB_AT_MOST(3,0)
	    NULL,
#endif
	    DB_INIT_MPOOL | DB_INIT_LOCK
	    | DB_INIT_LOG | DB_INIT_TXN | DB_CREATE | flags, /* mode */ 0644);
    if (ret != 0) {
	dbe->close(dbe, 0);
	print_error(__FILE__, __LINE__, "DB_ENV->open, err: %d, %s", ret, db_strerror(ret));
	if (ret == DB_RUNRECOVERY) {
	    fprintf(stderr, "To recover, run: bogoutil -v -f \"%s\"\n",
		    bogohome);
	}
	exit(EXIT_FAILURE);
    }

    if (DEBUG_DATABASE(1))
	fprintf(dbgout, "DB_ENV->open(home=%s)\n", bogohome);

    init = true;
    return 0;
}

/* initialize data base, configure some lock table sizes
 * (which can be overridden in the DB_CONFIG file)
 * and lock the file to tell other parts we're initialized and
 * do not want recovery to stomp over us
 */
int db_init(void) {
    const u_int32_t numlocks = 16384;
    const u_int32_t numobjs = 16384;

    if (needs_recovery(bogohome)) {
	db_recover(0, 0);
    }

    db_try_glock(F_RDLCK, F_SETLKW);

    if (set_lock(bogohome)) {
	exit(EX_ERROR);
    }

    return db_xinit(numlocks, numobjs, /* flags */ 0);
}

int db_recover(int catastrophic, int force) {
    int ret;

    while((force || needs_recovery(bogohome))
	    && (db_try_glock(F_WRLCK, F_SETLKW) <= 0))
	rand_sleep(10000,1000000);

    if (!(force || needs_recovery(bogohome)))
	return 0;

retry:
    if (DEBUG_DATABASE(0))
        fprintf(dbgout, "running %s data base recovery\n",
	    catastrophic ? "catastrophic" : "regular");
    ret = db_xinit(1024, 1024, catastrophic ? DB_RECOVER_FATAL : DB_RECOVER);
    if (ret) {
	if(!catastrophic) {
	    catastrophic = 1;
	    goto retry;
	}
	goto rec_fail;
    }

    clear_lockfile(bogohome);
    ds_cleanup();

    return 0;

rec_fail:
    exit(EX_ERROR);
}

void db_cleanup(void) {
    if (!init)
	return;
    if (dbe) {
	int ret;

	/* checkpoint if more than 64 kB of logs have been written
	 * or 120 min have passed since the previous checkpoint */
	/*                           kB  min flags */
	ret = BF_TXN_CHECKPOINT(dbe, 64, 120, 0);
	ret = db_flush_dirty(dbe, ret);
	if (ret)
	    print_error(__FILE__, __LINE__, "(db) DBE->txn_checkpoint returned %s", db_strerror(ret));

	ret = dbe->close(dbe, 0);
	if (DEBUG_DATABASE(1) || ret)
	    fprintf(dbgout, "DB_ENV->close(%p): %s\n", (void *)dbe, db_strerror(ret));
    }
    clear_lock();
    if (lockfd >= 0)
	close(lockfd); /* release locks */
    dbe = NULL;
    init = false;
}
