/* $Id$ */

/*****************************************************************************

NAME:
datastore.h - API for bogofilter datastore.  

   The idea here is to make bogofilter independent of the database
   system used to store words.  The interface specified by this file
   determines the entire interaction between bogofilter and the
   database.  Writing a new database backend merely requires the
   implementation of the interface.

AUTHORS:
Gyepi Sam <gyepi@praxis-sw.com>   2002 - 2003
Matthias Andree <matthias.andree@gmx.de> 2003

******************************************************************************/

#ifndef DATASTORE_TDB_H
#define DATASTORE_TDB_H

#ifndef	DB_NOTFOUND
/* Next line is from /usr/include/db3.h */
#define	DB_NOTFOUND		(-30990)/* Key/data pair not found (EOF). */
#endif

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
void  db_close(/*@only@*/ void *vhandle, bool nosync  /** Normally false, if true, do not synchronize data. This should not be used in regular operation but only to ease the disk I/O load when the lock operation failed. */);

/** Flush pending writes to disk */
void db_flush(dsh_t *dsh);

/** Cleanup storage allocation */
void db_cleanup(void);

/** Retrieve the value associated with a given word in a list.
 * \return zero if the word does not exist in the database. Front-end
 */
bool db_getvalues(dsh_t *dsh, const dbv_t *key, dbv_t *val);

/** Retrieve the value associated with a given word in a list. 
 * \return zero if the word does not exist in the database. Implementation
 */
int db_get_dbvalue(dsh_t *dsh, const dbv_t *token, /*@out@*/ dbv_t *val);

/** Delete the key */
int db_delete(dsh_t *dsh, const dbv_t *data);

/** Set the value associated with a given word in a list. Front end */
int db_setvalues(dsh_t *dsh, const dbv_t *token, dbv_t *val);

/** Set the value associated with a given word in a list. Implementation */
int db_set_dbvalue(dsh_t *dsh, const dbv_t *token, dbv_t *val);

/** Update the value associated with a given word in a list */
void db_updvalues(dsh_t *dsh, const dbv_t *token, const dbv_t *updval);

/** Iterate over all elements in data base and call \p hook for each item.
 * \p userdata is passed through to the hook function unaltered.
 */
typedef int (*db_foreach_t)(dbv_t *token, dbv_t *data, void *userdata);
int db_foreach(dsh_t *dsh, db_foreach_t hook, void *userdata);

/* Get the current process id */
unsigned long db_handle_pid(dsh_t *dsh);

/* Get the database filename */
char *db_handle_filename(dsh_t *dsh);

/* Prints wordlist name(s) */
void dbh_print_names(dsh_t *dsh, const char *msg);

/* Returns version string */
const char *db_version_str(void);

#endif
