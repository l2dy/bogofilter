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

static DB_ENV *dbe; /* libdb environment, if in use, NULL otherwise */

static const DBTYPE dbtype = DB_BTREE;

static bool init;

typedef struct {
    char	*path;
    char	*name;
    int		fd;
    dbmode_t	open_mode;
    DB		*dbp;
    bool	locked;
    bool	is_swapped;
} dbh_t;

#define DBT_init(dbt) (memset(&dbt, 0, sizeof(DBT)))

#define DB_AT_LEAST(maj, min) ((DB_VERSION_MAJOR > (maj)) || ((DB_VERSION_MAJOR == (maj)) && (DB_VERSION_MINOR >= (min))))
#define DB_AT_MOST(maj, min) ((DB_VERSION_MAJOR < (maj)) || ((DB_VERSION_MAJOR == (maj)) && (DB_VERSION_MINOR <= (min))))

/* Function definitions */

static const char *resolveflags(u_int32_t flags) {
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

static int DB_OPEN(DB *db, const char *file,
	const char *database, DBTYPE type, u_int32_t flags, int mode)
{
    int ret;

    ret = db->open(db,
#if DB_AT_LEAST(4,1)
	    NULL,
#endif
	    file, database, type, flags, mode);

    if (DEBUG_DATABASE(1) || getenv("BF_DEBUG_DB_OPEN"))
	fprintf(dbgout, "[pid %lu] DB->open(db=%p, file=%s, database=%s, type=%x, flags=%#lx=%s, mode=%#o) -> %d %s\n", (unsigned long)getpid(), db, file, database ? database : "NIL", type, (unsigned long)flags, resolveflags(flags), mode, ret, db_strerror(ret));

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


static dbh_t *dbh_init(const char *path, const char *name)
{
    dbh_t *handle;
    size_t len = strlen(path) + strlen(name) + 2;

    handle = xmalloc(sizeof(dbh_t));
    memset(handle, 0, sizeof(dbh_t));	/* valgrind */

    handle->fd   = -1; /* for lock */

    handle->path = xstrdup(path);
    handle->name = xmalloc(len);
    build_path(handle->name, len, path, name);

    handle->locked     = false;
    handle->is_swapped = false;

    return handle;
}


static void dbh_free(/*@only@*/ dbh_t *handle)
{
    if (handle != NULL) {
	xfree(handle->name);
	xfree(handle->path);
	xfree(handle);
    }
    return;
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

/* The old, pre-3.3 API will not fill in the page size with
 * DB_CACHED_COUNTS, and without DB_CACHED_COUNTS, BerlekeyDB will read
 * the whole data base, incurring a severe performance penalty. We'll
 * guess a page size.  As this is a safety margin for the file size,
 * we'll let the code below guess some size. */
#if DB_AT_LEAST(3,3)
/* return page size, of 0xffffffff for trouble */
static uint32_t get_psize(DB *dbp)
{
    uint32_t ret, pagesize;
    DB_BTREE_STAT *dbstat = NULL;

    ret = dbp->stat(dbp, &dbstat, DB_FAST_STAT);
    if (ret) {
	dbp->err (dbp, ret, "%s (db) DB->stat", progname);
	return 0xffffffff;
    }
    pagesize = dbstat->bt_pagesize;
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

/*
  Initialize database.
  Returns: pointer to database handle on success, NULL otherwise.
*/
void *db_open(const char *db_file, const char *name, dbmode_t open_mode)
{
    int ret;
    int is_swapped;
    int retries = 2; /* how often do we retry to open after ENOENT+EEXIST
			races? 2 is sufficient unless the kernel or
			BerkeleyDB are buggy. */
    char *t;

    dbh_t *handle = NULL;
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

    if (!init) abort();

    check_db_version();

    if (open_mode == DB_READ)
	opt_flags = DB_RDONLY;
    else
	/* Read-write mode implied.  Allow database to be created if
	 * necessary. DB_EXCL makes sure our locking doesn't fail if two
	 * applications try to create a DB at the same time. */
	opt_flags = 0;

    /* retry when locking failed */
    for (idx = 0; idx < COUNTOF(retryflags); idx += 1)
    {
	DB *dbp;
	uint32_t retryflag = retryflags[idx], pagesize;

	handle = dbh_init(db_file, name);

	if (handle == NULL)
	    break;

	/* create DB handle */
	if ((ret = db_create (&dbp, dbe, 0)) != 0) {
	    print_error(__FILE__, __LINE__, "(db) db_create, err: %d, %s",
			ret, db_strerror(ret));
	    goto open_err;
	}

	handle->dbp = dbp;

	/* set cache size, but only unless we're using an environment */
	if (dbe == NULL && db_cachesize != 0 &&
	    (ret = dbp->set_cachesize(dbp, db_cachesize/1024, (db_cachesize % 1024) * 1024*1024, 1)) != 0) {
	    print_error(__FILE__, __LINE__, "(db) DB(%s)->set_cachesize(%u,%u,%u), err: %d, %s",
			handle->name, db_cachesize/1024u, (db_cachesize % 1024u) * 1024u*1024u, 1u, ret, db_strerror(ret));
	    goto open_err;
	}

	/* open data base */
	if (dbe && (t = strrchr(handle->name, DIRSEP_C)))
	    t++;
	else
	    t = handle->name;

retry_db_open:
	if ((ret = DB_OPEN(dbp, t, NULL, dbtype, opt_flags | retryflag, 0664)) != 0
	    && ( ret != ENOENT || opt_flags == DB_RDONLY ||
		(ret = DB_OPEN(dbp, t, NULL, dbtype, opt_flags | DB_CREATE | DB_EXCL | retryflag, 0664)) != 0))
	{
	    if (opt_flags != DB_RDONLY && ret == EEXIST && --retries) {
		/* sleep for 4 to 100 ms - this is just to give up the CPU
		 * to another process and let it create the data base
		 * file in peace */
		rand_sleep(4 * 1000, 100 * 1000);
		goto retry_db_open;
	    }

	    /* close again and bail out without further tries */
	    print_error(__FILE__, __LINE__, "(db) DB->open(%s) - actually %s bogohome: %s -, err: %d, %s",
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
	    dbp->err (dbp, ret, "%s (db) DB->get_byteswapped: %s",
		      progname, handle->name);
	    db_close(handle, false);
	    return NULL;		/* handle already freed, ok to return */
	}

	ret = dbp->fd(dbp, &handle->fd);
	if (ret != 0) {
	    dbp->err (dbp, ret, "%s (db) DB->fd: %s",
		      progname, handle->name);
	    db_close(handle, false);
	    return NULL;		/* handle already freed, ok to return */
	}

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

	/* skip manual lock when run in environment */
	if (dbe)
	    break;

	/* try fcntl lock */
	if (db_lock(handle->fd, F_SETLK,
		    (short int)(open_mode == DB_READ ? F_RDLCK : F_WRLCK)))
	{
	    int e = errno;
	    db_close(handle, true);
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
	dsh_t *dsh;
	handle->locked = true;
	if (handle->fd < 0)
	    handle->locked=false;
	dsh = dsh_init(handle, handle->is_swapped);
	return (void *)dsh;
    }

    return NULL;

 open_err:
    dbh_free(handle);

    if (ret >= 0)
	errno = ret;
    else
	errno = EINVAL;
    return NULL;
}


int db_delete(dsh_t *dsh, const dbv_t *token)
{
    int ret = 0;
    dbh_t *handle = dsh->dbh;
    DB *dbp = handle->dbp;

    DBT db_key;
    DBT_init(db_key);

    db_key.data = token->data;
    db_key.size = token->leng;

    ret = dbp->del(dbp, NULL, &db_key, 0);

    if (ret != 0 && ret != DB_NOTFOUND) {
	print_error(__FILE__, __LINE__, "(db) db_delete('%.*s'), err: %d, %s",
		    CLAMP_INT_MAX(db_key.size),
		    (const char *) db_key.data,
    		    ret, db_strerror(ret));
	exit(EX_ERROR);
    }

    return ret;		/* 0 if ok */
}


int db_get_dbvalue(dsh_t *dsh, const dbv_t *token, /*@out@*/ dbv_t *val)
{
    int ret = 0;
    DBT db_key;
    DBT db_data;

    dbh_t *handle = dsh->dbh;
    DB *dbp = handle->dbp;

    DBT_init(db_key);
    DBT_init(db_data);

    db_key.data = token->data;
    db_key.size = token->leng;

    db_data.data = val->data;
    db_data.size = val->leng;		/* cur used */
    db_data.ulen = val->leng;		/* max size */
    db_data.flags = DB_DBT_USERMEM;	/* saves the memcpy */

    ret = dbp->get(dbp, NULL, &db_key, &db_data, 0);

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
    DB *dbp = handle->dbp;

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
    int ret;
    dbh_t *handle = vhandle;
    DB *dbp = handle->dbp;
    uint32_t f = nosync ? DB_NOSYNC : 0;

    if (DEBUG_DATABASE(1))
	fprintf(dbgout, "db_close (%s) %s\n",
		handle->name, nosync ? "nosync" : "sync");

    ret = dbp->close(dbp, f);
#if DB_AT_LEAST(3,2) && DB_AT_MOST(4,0)
    /* ignore dirty pages in buffer pool */
    if (ret == DB_INCOMPLETE)
	ret = 0;
#endif
    if (ret)
	print_error(__FILE__, __LINE__, "(db) db_close err: %d, %s", ret, db_strerror(ret));

    dbh_free(handle);
}


/*
 flush any data in memory to disk
*/
void db_flush(dsh_t *dsh)
{
    int ret;
    dbh_t *handle = dsh->dbh;
    DB *dbp = handle->dbp;

    ret = dbp->sync(dbp, 0);
#if DB_AT_LEAST(3,2) && DB_AT_MOST(4,0)
    /* ignore dirty pages in buffer pool */
    if (ret == DB_INCOMPLETE)
	ret = 0;
#endif
    if (ret)
	print_error(__FILE__, __LINE__, "(db) db_sync: err: %d, %s", ret, db_strerror(ret));
}


int db_foreach(dsh_t *dsh, db_foreach_t hook, void *userdata)
{
    dbh_t *handle = dsh->dbh;
    DB *dbp = handle->dbp;

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

/* dummy infrastructure, to be expanded by environment
 * or transactional initialization/shutdown */
static bool init = false;
int db_init(void) {
    char *t;
    int cdb_alldb = 1;

    if (!bogohome)
	abort();

    if (bogohome && getenv("BF_EXPERIMENTAL_DBENV")) {
	int ret = db_env_create(&dbe, 0);
	if (ret != 0) {
	    print_error(__FILE__, __LINE__, "db_env_create, err: %d, %s", ret, db_strerror(ret));
	    abort();
	}
	if (db_cachesize != 0 &&
	    (ret = dbe->set_cachesize(dbe, db_cachesize/1024, (db_cachesize % 1024) * 1024*1024, 1)) != 0) {
	    print_error(__FILE__, __LINE__, "DBENV->set_cachesize(%u), err: %d, %s",
			db_cachesize, ret, db_strerror(ret));
	    exit(EXIT_FAILURE);
	}

	/* Allow user to override DB_CDB_ALLDB to 0 */
	if ((t = getenv("DB_CDB_ALLDB")))
	    cdb_alldb = atoi(t);

	if ((ret = dbe->set_shm_key(dbe, 0xBFDB42))) {
	    dbe->close(dbe, 0);
	    print_error(__FILE__, __LINE__, "DBENV->set_shm_key, err: %d, %s", ret, db_strerror(ret));
	    exit(EXIT_FAILURE);
	}

	ret = dbe->open(dbe, bogohome, DB_INIT_MPOOL | DB_SYSTEM_MEM | DB_INIT_CDB | DB_CREATE, /* mode */ 0644);
	if (ret != 0) {
	    dbe->close(dbe, 0);
	    print_error(__FILE__, __LINE__, "DBENV->open, err: %d, %s", ret, db_strerror(ret));
	    exit(EXIT_FAILURE);
	}
    }
    init = true;
    return 0;
}

void db_cleanup(void) {
    if (dbe)
	dbe->close(dbe, 0);
    dbe = NULL;
    init = false;
}
