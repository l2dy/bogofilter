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
    NULL,
    NULL,
    NULL,

    /* private -- used in datastore_db_*.c */
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};
