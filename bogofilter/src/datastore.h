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

#include "system.h"
#include "word.h"
#include "wordlists.h"

#define MSG_COUNT_TOK ((const byte *)".MSG_COUNT")

#undef UINT32_MAX
#define UINT32_MAX 4294967295lu /* 2 ^ 32 - 1 */

typedef struct {
    uint32_t count[2];		/* spam and ham counts */
    uint32_t date;
} dbv_t;

/** Initialize database, open and lock files, etc.
 * params: char * path to database file, char * name of database
 * \return opaque pointer to database handle, which must be saved and
 * passed as the first parameter in all subsequent database function calls. 
 */
/*@only@*/ /*@null@*/
void *db_open(const char *path	/** path to database file */, 
	      size_t count	/** number of data base(s) */,
	      const char **name /** name(s) of data base(s) */,
	      dbmode_t mode	/** open mode, DB_READ or DB_WRITE */);

/** Close file and clean up. */
void  db_close(/*@only@*/ void *, bool nosync  /** Normally false, if true, do not synchronize data. This should not be used in regular operation but only to ease the disk I/O load when the lock operation failed. */);

/** Flush pending writes to disk */
void db_flush(void *);

/** Cleanup storage allocation */
void db_cleanup(void);

/** Increments count for given word.  Note: negative results are set to
 * zero.
 */
void db_increment(void *, const word_t *, dbv_t *);

/** Decrement count for a given word, if it exists in the datastore.
 * Note: negative results are set to zero. 
 */
void db_decrement(void *, const word_t *, dbv_t *);

/** Retrieve the value associated with a given word in a list. 
 * \return zero if the word does not exist in the database. Front-end
 */
bool db_getvalues(void *, const word_t *, dbv_t *);

/** Retrieve the value associated with a given word in a list. 
 * \return zero if the word does not exist in the database. Implementation
 */
int db_get_dbvalue(void *vhandle, const word_t *word, dbv_t *val);

/** Delete the key */
void db_delete(void *, const word_t *);

/** Set the value associated with a given word in a list. Front end */
void db_setvalues(void *, const word_t *, dbv_t *);

/** Set the value associated with a given word in a list. Implementation */
void db_set_dbvalue(void *vhandle, const word_t *word, dbv_t *val);

/** Update the value associated with a given word in a list */
void db_updvalues(void *vhandle, const word_t *word, const dbv_t *updval);

/** Get the database message count */
void db_get_msgcounts(void*, dbv_t *);

/** set the database message count */
void db_set_msgcounts(void*, dbv_t *);

typedef int (*db_foreach_t)(word_t *w_key, word_t *w_value, void *userdata);
/** Iterate over all elements in data base and call \p hook for each item.
 * \p userdata is passed through to the hook function unaltered.
 */
int db_foreach(void *, db_foreach_t hook, void *userdata);

/* Get the current process id */
unsigned long db_handle_pid(void *);

/* Get the database filename */
char *db_handle_filename(void *);

/* Locks and unlocks file descriptor */
int db_lock(int fd, int cmd, short int type);

/* Prints wordlist name(s) */
void dbh_print_names(void *vhandle, const char *msg);

/* This is not currently used ...
 * 
#define db_write_lock(fd) db_lock(fd, F_SETLKW, F_WRLCK)
#define db_read_lock(fd) db_lock(fd, F_SETLKW, F_RDLCK)
#define db_unlock(fd) db_lock(fd, F_SETLK, F_UNLCK)

*/

#endif
