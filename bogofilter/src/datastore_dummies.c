
#include "common.h"
#include "datastore.h"
#include "datastore_db.h"

void *dbe_init(bfdir *d, bffile *f) {
    (void)d;
    (void)f;
    return (void *)~0;
}

void dsm_init(bfdir *unused1, bffile *unused2) {
    (void)unused1;
    (void)unused2;
}

void *db_get_env(void *vhandle) {
    (void)vhandle;
    return NULL;
}
