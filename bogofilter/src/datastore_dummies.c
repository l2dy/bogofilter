/** \file datastore_dummies.c
 * Dummy functions for non-transactional data stores
 */

#include "common.h"
#include "datastore_db.h"

#include <stdlib.h>

void *dbe_init(const char *unused) {
    (void)unused;
    return (void *)~0;
}

void dbe_cleanup(void *vhandle) {
    (void)vhandle;
}

int db_txn_begin(void *vhandle) { (void)vhandle; return 0; }
int db_txn_abort(void *vhandle) { (void)vhandle; return 0; }
int db_txn_commit(void *vhandle) { (void)vhandle; return 0; }
ex_t dbe_recover(const char *d, bool a, bool b) {
    (void)d;
    (void)a;
    (void)b;

    fprintf(stderr,
	    "ERROR: bogofilter can not recover databases without transaction support.\n"
	    "If you experience hangs, strange behavior, inaccurate output,\n"
	    "you must delete your data base and rebuild it, or restore an older version\n"
	    "that you know is good from your backups.\n");
    exit(EX_ERROR);
}

void *db_get_env(void *vhandle) {
    (void)vhandle;
    return NULL;
}

ex_t dbe_remove(const char *d) {
    (void)d;
    return EX_OK;
}

ex_t dbe_purgelogs(const char *d) {
    (void)d;
    return EX_OK;
}
