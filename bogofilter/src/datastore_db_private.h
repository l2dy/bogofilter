/* $Id$ */

/*****************************************************************************

NAME:

datastore_db_private.h - provide OO interface for datastore methods;
		       	 used for Berkeley DB transactional &
		       	 non-transactional support.

AUTHOR:
David Relson	<relson@osagesoftware.com> 2005

******************************************************************************/

#ifndef DATASTORE_DB_PRIVATE_H
#define DATASTORE_DB_PRIVATE_H

#define MAGIC_DBE 0xdbe
#define MAGIC_DBH 0xdb4

#ifndef	ENABLE_DB_DATASTORE	/* if not Berkeley DB */
typedef	void DB;
typedef	void DB_ENV;
typedef	void DB_TXN;
#endif

/** implementation internal type to keep track of database environments
 * we have opened. */
typedef struct {
    int		magic;
    DB_ENV	*dbe;		/* stores the environment handle */
    char	*directory;	/* stores the home directory for this environment */
} dbe_t;

/* public -- used in datastore.c */
typedef int	dsm_i_pv	(void *vhandle);
/* private -- used in datastore_db_*.c */
typedef int	dsm_i_i		(int open_mode);
typedef int	dsm_i_pnvi	(DB_ENV *dbe, int ret);
typedef int	dsm_i_pvi	(void *handle, int open_mode);
typedef int	dsm_i_v		(void);
typedef void	dsm_v_pbe	(dbe_t *env);
typedef void	dsm_v_pnv	(DB_ENV *dbe);
typedef void	dsm_v_pvuiui	(void *vhandle, u_int32_t numlocks, u_int32_t numobjs);
typedef const char *dsm_pc_pc	(const char *db_file);
typedef ex_t	dsm_x_pnvpd	(DB_ENV *dbe, bfdir *directory);
typedef dbe_t  *dsm_pbe_pd	(bfdir *directory);
typedef DB_ENV *dsm_pnv_pdpf	(bfdir *directory, bffile *db_file);
typedef DB_ENV *dsm_pnv_pbe	(dbe_t *env);

typedef struct {
    /* public -- used in datastore.c */
    dsm_i_pv	 *dsm_begin;
    dsm_i_pv	 *dsm_abort;
    dsm_i_pv	 *dsm_commit;
    /* private -- used in datastore_db_*.c */
    dsm_pbe_pd	 *dsm_env_init;
    dsm_v_pbe	 *dsm_cleanup;
    dsm_v_pbe	 *dsm_cleanup_lite;
    dsm_pnv_pbe	 *dsm_get_env_dbe;
    dsm_pc_pc	 *dsm_database_name;
    dsm_pnv_pdpf *dsm_recover_open; /**< exits on failure */
    dsm_i_v	 *dsm_auto_commit_flags;
    dsm_i_i	 *dsm_get_rmw_flag;
    dsm_i_pvi	 *dsm_lock;
    dsm_x_pnvpd	 *dsm_common_close;
    dsm_i_pnvi	 *dsm_sync;
    dsm_v_pnv	 *dsm_log_flush;
} dsm_t;

/** implementation internal type to keep track of databases
 * we have opened. */

#ifndef	ENABLE_SQLITE_DATASTORE
typedef struct {
    int		magic;
    bfdir	path;
    bffile	name;
    int		fd;		/* file descriptor of data base file */
    dbmode_t	open_mode;	/* datastore open mode, DS_READ/DS_WRITE */
#ifdef	ENABLE_DB_DATASTORE	/* if Berkeley DB */
    DB		*dbp;		/* data base handle */
#endif
    bool	locked;
    bool	is_swapped;	/* set if CPU and data base endianness differ */
    bool	created;	/* if newly created; for datastore.c (to add .WORDLIST_VERSION) */
    dbe_t	*dbenv;		/* "parent" environment */
#ifdef	ENABLE_DB_DATASTORE	/* if Berkeley DB */
    DB_TXN	*txn;		/* transaction in progress or NULL */
#endif
    /** OO database methods */
    dsm_t	*dsm;
} dbh_t;
#endif

#define DBT_init(dbt)		(memset(&dbt, 0, sizeof(DBT)))

typedef	enum {
    P_ERROR = -1,	/* -1 for error */
    P_DISABLE = 0,	/*  0 for no transactions */
    P_ENABLE  = 1,	/*  1 for transactions */
    P_DONT_KNOW		/*  2 for don't know */
} probe_txn_t;

probe_txn_t probe_txn(bfdir *directory, bffile *file);

#endif
