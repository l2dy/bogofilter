/* $Id$ */

/*****************************************************************************

NAME:
datastore_db.c -- implements the datastore, using Berkeley DB

AUTHORS:
Gyepi Sam <gyepi@praxis-sw.com>   2002 - 2003
Matthias Andree <matthias.andree@gmx.de> 2003

******************************************************************************/

/* To avoid header file conflicts the order is:
**	1. System header files
**	2. Header files for external packages
**	3. Bogofilter's header files
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

static const DBTYPE dbtype = DB_BTREE;

/* dummy variables */
u_int32_t db_max_objects, db_max_locks;

typedef struct {
    char	*path;
    char	*name;
    int		fd;		/* file descriptor of data base file */
    dbmode_t	open_mode;	/* datastore open mode, DS_READ/DS_WRITE */
    DB		*dbp;		/* data base handle */
    bool	locked;
    bool	is_swapped;	/* set if CPU and data base endianness differ */
    DB_TXN	*txn;		/* stores the transaction handle */
    bool	created;	/* if newly created; for datastore.c (to add .WORDLIST_VERSION) */
} dbh_t;

#define DBT_init(dbt) (memset(&dbt, 0, sizeof(DBT)))

/* dummy infrastructure, to be expanded by environment
 * or transactional initialization/shutdown */

/* Function definitions */

/** translate BerkeleyDB \a flags bitfield back to symbols */
static const char *resolveopenflags(u_int32_t flags) {
    static char buf[160];
    char b2[80];
    strlcpy(buf, "", sizeof(buf));
    if (flags & DB_CREATE) flags &= ~DB_CREATE, strlcat(buf, "DB_CREATE ", sizeof(buf));
    if (flags & DB_EXCL) flags &= ~DB_EXCL, strlcat(buf, "DB_EXCL ", sizeof(buf));
    if (flags & DB_NOMMAP) flags &= ~DB_NOMMAP, strlcat(buf, "DB_NOMMAP ", sizeof(buf));
    if (flags & DB_RDONLY) flags &= ~DB_RDONLY, strlcat(buf, "DB_RDONLY ", sizeof(buf));
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


/** "constructor" - allocate our handle and initialize its contents */
static dbh_t *handle_init(const char *path, const char *name)
{
    dbh_t *handle;
    size_t len = strlen(path) + strlen(name) + 2;

    handle = xmalloc(sizeof(dbh_t));
    memset(handle, 0, sizeof(dbh_t));	/* valgrind */

    handle->fd   = -1;			/* for lock */

    handle->path = xstrdup(path);
    handle->name = xmalloc(len);
    build_path(handle->name, len, path, name);

    handle->locked     = false;
    handle->is_swapped = false;
    handle->created    = false;

    handle->txn = NULL;

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

#if DB_AT_MOST(3,0)
#error "Berkeley DB 3.0 is not supported"
#endif

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
    static char v[80];
    snprintf(v, sizeof(v), "Berkeley DB (%d.%d.%d) NONTRANSACTIONAL",
	    DB_VERSION_MAJOR, DB_VERSION_MINOR, DB_VERSION_PATCH);
    return v;
}

/** Initialize database. Expects open environment.
 * \return pointer to database handle on success, NULL otherwise.
 */
void *db_open(void *dummy, const char *path, const char *name, dbmode_t open_mode)
{
    int ret;
    int is_swapped;
    int retries = 2; /* how often do we retry to open after ENOENT+EEXIST
			races? 2 is sufficient unless the kernel or
			BerkeleyDB are buggy. */
    char *t;

    dbh_t *handle = NULL;
    uint32_t opt_flags = (open_mode == DS_READ) ? DB_RDONLY : 0;

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

    (void)dummy;
    check_db_version();

    /* retry when locking failed */
    for (idx = 0; idx < COUNTOF(retryflags); idx += 1)
    {
	DB *dbp;
	bool err = false;
	uint32_t pagesize;
	uint32_t retryflag = retryflags[idx];

	handle = handle_init(path, name);

	if (handle == NULL)
	    return NULL;

	/* create DB handle */
	if ((ret = db_create (&dbp, NULL, 0)) != 0) {
	    print_error(__FILE__, __LINE__, "db_create, err: %d, %s",
			ret, db_strerror(ret));
	    goto open_err;
	}

	handle->dbp = dbp;

	/* set cache size, but not when we're using an environment */
	if (db_cachesize != 0 &&
	    (ret = dbp->set_cachesize(dbp, db_cachesize/1024, (db_cachesize % 1024) * 1024*1024, 1)) != 0) {
	    print_error(__FILE__, __LINE__, "db(%s)->set_cachesize(%u,%u,%u), err: %d, %s",
			handle->name, db_cachesize/1024u, (db_cachesize % 1024u) * 1024u*1024u, 1u, ret, db_strerror(ret));
	    goto open_err;
	}

	/* open data base */
	t = handle->name;

retry_db_open:
	handle->created = false;
	ret = DB_OPEN(dbp, t, NULL, dbtype, opt_flags | retryflag, 0664);

	if (ret != 0) {
	    err = (ret != ENOENT) || (opt_flags == DB_RDONLY);
	    if (!err) {
		ret = DB_OPEN(dbp, t, NULL, dbtype, opt_flags | DB_CREATE | DB_EXCL | retryflag, 0664);
		if (ret != 0)
		    err = true;
		else
		    handle->created = true;
	    }
	}

	if (ret != 0) {
	    if (ret == ENOENT && opt_flags != DB_RDONLY)
		return NULL;
	    else
		err = true;
	}

	if (err)
	{
	    if (opt_flags != DB_RDONLY && ret == EEXIST && --retries) {
		/* sleep for 4 to 100 ms - this is just to give up the CPU
		 * to another process and let it create the data base
		 * file in peace */
		rand_sleep(4 * 1000, 100 * 1000);
		goto retry_db_open;
	    }

	    /* close again and bail out without further tries */
	    if (DEBUG_DATABASE(0))
		print_error(__FILE__, __LINE__, "DB->open(%s) - actually %s, bogohome %s, err %d, %s",
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

	/* try fcntl lock */
	if (db_lock(handle->fd, F_SETLK,
		    (short int)(open_mode == DS_READ ? F_RDLCK : F_WRLCK)))
	{
	    int e = errno;
	    db_close(handle);
	    handle = NULL;	/* db_close freed it, we don't want to use it anymore */
	    errno = e;
	    if (errno == EACCES)
		errno = EAGAIN;
	    if (errno != EAGAIN)
		return NULL;
	} else {
	    /* have lock */
	    break;
	}
    } /* for idx over retryflags */

    if (handle) {
	handle->locked = true;
	if (handle->fd < 0)
	    handle->locked=false;
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


int db_delete(void *vhandle, const dbv_t *token)
{
    int ret = 0;
    dbh_t *handle = vhandle;
    DB *dbp = handle->dbp;

    DBT db_key;
    DBT_init(db_key);

    db_key.data = token->data;
    db_key.size = token->leng;

    ret = dbp->del(dbp, handle->txn, &db_key, 0);

    if (ret != 0 && ret != DB_NOTFOUND) {
	print_error(__FILE__, __LINE__, "DB->del('%.*s'), err: %d, %s",
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

    DBT_init(db_key);
    DBT_init(db_data);

    db_key.data = token->data;
    db_key.size = token->leng;

    db_data.data = val->data;
    db_data.size = val->leng;		/* cur used */
    db_data.ulen = val->leng;		/* max size */
    db_data.flags = DB_DBT_USERMEM;	/* saves the memcpy */

    /* DB_RMW can avoid deadlocks */
    ret = dbp->get(dbp, handle->txn, &db_key, &db_data, 0);

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
    default:
	print_error(__FILE__, __LINE__, "DB->get('%.*s' ), err: %d, %s",
		    CLAMP_INT_MAX(token->leng), (char *) token->data, ret, db_strerror(ret));
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

    DBT_init(db_key);
    DBT_init(db_data);

    db_key.data = token->data;
    db_key.size = token->leng;

    db_data.data = val->data;
    db_data.size = val->leng;		/* write count */

    ret = dbp->put(dbp, handle->txn, &db_key, &db_data, 0);

    if (ret != 0) {
	print_error(__FILE__, __LINE__, "db_set_dbvalue( '%.*s' ), err: %d, %s",
		    CLAMP_INT_MAX(token->leng), (char *)token->data, ret, db_strerror(ret));
	exit(EX_ERROR);
    }

    if (DEBUG_DATABASE(3))
	fprintf(dbgout, "DB->put(%.*s): %s\n",
		CLAMP_INT_MAX(token->leng), (char *) token->data, db_strerror(ret));

    return 0;
}


/* Close files and clean up. */
void db_close(void *vhandle)
{
    int ret;
    dbh_t *handle = vhandle;
    DB *dbp = handle->dbp;

    if (DEBUG_DATABASE(1))
    	fprintf(dbgout, "DB->close(%s)\n",
		handle->name);

    ret = dbp->close(dbp, 0);
#if DB_AT_LEAST(3,2) && DB_AT_MOST(4,0)
    /* ignore dirty pages in buffer pool */
    if (ret == DB_INCOMPLETE)
	ret = 0;
#endif
    if (ret)
	print_error(__FILE__, __LINE__, "DB->close error: %s",
		db_strerror(ret));

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

    if (DEBUG_DATABASE(1))
	fprintf(dbgout, "db_flush(%s)\n", handle->name);

    ret = dbp->sync(dbp, 0);
#if DB_AT_LEAST(3,2) && DB_AT_MOST(4,0)
    /* ignore dirty pages in buffer pool */
    if (ret == DB_INCOMPLETE)
	ret = 0;
#endif
    if (ret)
	print_error(__FILE__, __LINE__, "db_sync: err: %d, %s", ret, db_strerror(ret));
}


ex_t db_foreach(void *vhandle, db_foreach_t hook, void *userdata)
{
    dbh_t *handle = vhandle;
    DB *dbp = handle->dbp;

    ex_t ret = 0;
    bool eflag = false;

    DBC dbc;
    DBC *dbcp = &dbc;
    DBT key, data;

    dbv_t dbv_key, dbv_data;
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

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

ex_t db_verify(const char *file) {
    DB *dbp;
    int e;
    int fd = open(file, O_RDWR);

    if (fd < 0) {
	print_error(__FILE__, __LINE__, "db_verify: cannot open %s: %s", file,
	       strerror(errno));
	exit(EX_ERROR);
    }

    if (db_lock(fd, F_SETLKW, (short int)F_WRLCK)) {
	print_error(__FILE__, __LINE__,
		"db_verify: cannot lock %s for exclusive use: %s", file,
		strerror(errno));
	close(fd);
	exit(EX_ERROR);
    }

    if ((e = db_create (&dbp, NULL, 0)) != 0) {
	print_error(__FILE__, __LINE__, "db_create, err: %s",
		db_strerror(e));
	close(fd);
	exit(EX_ERROR);
    }

    e = dbp->verify(dbp, file, NULL, NULL, 0);
    if (e) {
	print_error(__FILE__, __LINE__, "database %s does not verify: %s",
		file, db_strerror(e));
	exit(EX_ERROR);
    } else {
	if (verbose)
	    printf("%s OK.\n", file);
    }
    close(fd);
    return EX_OK;
}
