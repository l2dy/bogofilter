/* $Id$ */

/*****************************************************************************

NAME:
datastore_db_trad_stub.c -- stub routines for bogofilter's traditional
		       (non-transactional) datastore, 
		       using Berkeley DB

AUTHORS:
David Relson	<relson@osagesoftware.com> 2005

******************************************************************************/

#include "common.h"

#include <db.h>

#include "datastore.h"
#include "datastore_db.h"
#include "datastore_db_private.h"

/* OO function lists */

dsm_t dsm_traditional = {
    /* public -- used in datastore.c */
    NULL,	/* dsm_begin          */
    NULL,	/* dsm_abort          */
    NULL,	/* dsm_commit         */

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
    NULL,	/* dsm_pagesize         */
    NULL,	/* dsm_checkpoint       */
    NULL,	/* dsm_purgelogs        */
    NULL,	/* dsm_recover          */
    NULL,	/* dsm_remove           */
    NULL	/* dsm_verify           */
};
