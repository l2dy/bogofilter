/* $Id$ */

/*****************************************************************************

NAME:
datastore_db_trans_stub.c -- stub routines for bogofilter's transactional
		       datastore, using Berkeley DB

AUTHORS:

David Relson	<relson@osagesoftware.com> 2005

******************************************************************************/

#include "common.h"

#ifdef ENABLE_DB_DATASTORE
#include <db.h>
#endif

#include "datastore.h"
#include "datastore_db.h"
#include "datastore_db_private.h"

/** Default flags for DB_ENV->open() */

u_int32_t db_max_locks = 16384;		/* set_lk_max_locks    32768 */
u_int32_t db_max_objects = 16384;	/* set_lk_max_objects  32768 */

bool	  db_log_autoremove = false;	/* DB_LOG_AUTOREMOVE */

#ifdef	FUTURE_DB_OPTIONS
bool	  db_txn_durable = true;	/* not DB_TXN_NOT_DURABLE */
#endif

/* OO function lists */

dsm_t dsm_transactional = {
    /* public -- used in datastore.c */
    NULL,	/* dsm_begin           */
    NULL,	/* dsm_abort           */
    NULL,	/* dsm_commit          */
    /* private -- used in datastore_db_*.c */
    NULL,	/* dsm_env_init         */
    NULL,	/* dsm_cleanup          */
    NULL,	/* dsm_cleanup_lite     */
    NULL,	/* dsm_get_env_dbe      */
    NULL,	/* dsm_database_name    */
    NULL,	/* dsm_recover_open     */
    NULL,	/* dsm_auto_commit_flags*/                    
    NULL,	/* dsm_get_rmw_flag     */
    NULL,	/* dsm_lock             */
    NULL,	/* dsm_common_close     */
    NULL,	/* dsm_sync             */
    NULL,	/* dsm_log_flush        */
    NULL,	/* dsm_checkpoint       */
    NULL,	/* dsm_pagesize         */
    NULL,	/* dsm_purgelogs        */
    NULL,	/* dsm_recover          */
    NULL,	/* dsm_remove           */
    NULL	/* dsm_verify           */
};

#ifndef ENABLE_SQLITE_DATASTORE

#if 	!defined(DISABLE_TRANSACTIONS) && !defined(ENABLE_TRANSACTIONS)
void *dbe_init(bfdir *d, bffile *f) {
    (void)d;
    (void)f;
    return (void *)~0;
}
#endif

int db_txn_begin(void *vhandle) { (void)vhandle; return 0; }
int db_txn_abort(void *vhandle) { (void)vhandle; return 0; }
int db_txn_commit(void *vhandle) { (void)vhandle; return 0; }

#else

extern void *dsm;
void *dbe_init(bfdir *d, bffile *f) {
    dsm = &dsm_transactional;
    (void)d;
    (void)f;
    return (void *)~0;
}

#endif

ex_t dbe_recover(bfdir *directory, bool catastrophic, bool force)
{
    (void) directory;
    (void) catastrophic;
    (void) force;

    fprintf(stderr,
	    "ERROR: bogofilter can not recover databases without transaction support.\n"
	    "If you experience hangs, strange behavior, inaccurate output,\n"
	    "you must delete your data base and rebuild it, or restore an older version\n"
	    "that you know is good from your backups.\n");
    exit(EX_ERROR);
}

#if	!defined(DISABLE_TRANSACTIONS) && !defined(ENABLE_TRANSACTIONS)
void *db_get_env(void *vhandle) {
    (void)vhandle;
    return NULL;
}
#endif

ex_t dbe_checkpoint(bfdir *directory)
{
    (void) directory;
    return EX_OK;
}

ex_t dbe_purgelogs(bfdir *directory)
{
    (void) directory;
    return EX_OK;
}

ex_t dbe_remove(bfdir *directory)
{
    (void) directory;
    return EX_OK;
}

/** probe if the directory contains an environment, and if so,
 * if it has transactions
 */
probe_txn_t probe_txn(bfdir *directory, bffile *file)
{
    (void) directory;
    (void) file;
    return P_DISABLE;
}
