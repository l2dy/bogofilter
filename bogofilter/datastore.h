/**
 * \file datastore.h
 * API for bogofilter datastore.  The idea here is to make bogofilter
 * independent of the database system used to store words. The interface
 * specified by this file determines the entire interaction between
 * bogofilter and the database. Writing a new database backend merely
 * requires the implementation of the interface.
 *
 * \author Gyepi Sam  <gyepi@praxis-sw.com>
 * \author Matthias Andree <matthias.andree@gmx.de>
 * \date 2002-2003
 * $Id$
 */

#ifndef DATASTORE_H
#define DATASTORE_H

#include "wordlists.h"
#include <inttypes.h>

typedef struct {
    uint32_t count;
    uint32_t date;
} dbv_t;

/** Initialize database, open and lock files, etc.
 * params: char * path to database file, char * name of database
 * \return opaque pointer to database handle, which must be saved and
 * passed as the first parameter in all subsequent database function calls. 
 */
/*@only@*/ /*@null@*/
void *db_open(const char *path /** path to database file */, const char *name /** name of data base */,
	dbmode_t mode /** open mode, DB_READ or DB_WRITE */, const char *dir /** directory, currently unused */);

/** Close file and clean up. */
void  db_close(/*@only@*/ void *, bool nosync  /** Normally false, if true, do not synchronize data. This should not be used in regular operation but only to ease the disk I/O load when the lock operation failed. */);

/** Flush pending writes to disk */
void db_flush(void *);

/** Increments count for given word.  Note: negative results are set to
 * zero.
 */
void db_increment(void *, const char *, uint32_t);

/** Decrement count for a given word, if it exists in the datastore.
 * Note: negative results are set to zero. 
 */
void db_decrement(void *, const char *, uint32_t);

/** Retrieve the value associated with a given word in a list. 
 * \return zero if the word does not exist in the database. 
 */
uint32_t db_getvalue(void *, const char *);

/** Delete the key */
void db_delete(void *, const char *);

/** Set the value associated with a given word in a list */
void db_setvalue(void *, const char *, uint32_t);

/** Get the database message count */
uint32_t db_get_msgcount(void*);

/** set the database message count */
void db_set_msgcount(void*, uint32_t);

typedef int (*db_foreach_t)(char *key,   uint32_t key_size,
			    char *value, uint32_t key_value, 
			    void *userdata);
/** Iterate over all elements in data base and call \p hook for each item.
 * \p userdata is passed through to the hook function unaltered.
 */
int db_foreach(void *, db_foreach_t hook, void *userdata);

#endif
