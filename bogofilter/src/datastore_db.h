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

#ifndef DATASTORE_DB_H
#define DATASTORE_DB_H

#include "datastore.h"

extern	u_int32_t db_max_locks;		/* set_lk_max_locks   */
extern	u_int32_t db_max_objects;	/* set_lk_max_objects */

extern	bool	  db_log_autoremove;	/* DB_LOG_AUTOREMOVE  */
extern	bool	  db_txn_durable;	/* not DB_TXN_NOT_DURABLE */

/** Initialize database, open and lock files, etc.
 * params: char * path to database file, char * name of database
 * \return opaque pointer to database handle, which must be saved and
 * passed as the first parameter in all subsequent database function calls. 
 */
/*@only@*/ /*@null@*/
void *db_open(void *env,	/**< database environment to open DB in */
	      const char *path,	/**< path to database file */
	      const char *name,	/**< name(s) of data base(s) */
	      dbmode_t mode	/**< open mode, DS_READ or DS_WRITE */);

/** Close file and clean up. */
void  db_close(/*@only@*/ void *vhandle);

/** Flush pending writes to disk */
void db_flush(void *handle);

/** Do global initializations. \return pointer to environment for success, NULL for
 * error. */
void *dbe_init(const char *directory);

/** Cleanup storage allocation */
void dbe_cleanup(void *);

/** Retrieve the value associated with a given word in a list. 
 * \return zero if the word does not exist in the database. Implementation
 */
int db_get_dbvalue(void *handle, const dbv_t *token, /*@out@*/ dbv_t *val);

/** Delete the key */
int db_delete(void *handle, const dbv_t *data);

/** Set the value associated with a given word in a list. Implementation */
int db_set_dbvalue(void *handle, const dbv_t *token, dbv_t *val);

/** Iterate over all elements in data base and call \p hook for each item.
 * \p userdata is passed through to the hook function unaltered.
 */
typedef int (*db_foreach_t)(dbv_t *token, dbv_t *data, void *userdata);
int db_foreach(void *handle, db_foreach_t hook, void *userdata);

/* Returns error associated with code */
const char *db_str_err(int);

/* Returns version string */
const char *db_version_str(void);

/* Transactional interfaces */
int dbe_txn_begin(void *vhandle);
int dbe_txn_abort(void *vhandle);
int dbe_txn_commit(void *vhandle);

int dbe_recover(const char *directory, bool catastrophic, bool force);
int dbe_remove(const char *directory);

/** Returns is_swapped flag */
bool db_is_swapped(void *vhandle);

/** Returns created flag */
bool db_created(void *vhandle);

/** Returns parent environment */
void *db_get_env(void *vhandle);

#endif
