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

#ifndef DATASTORE_H
#define DATASTORE_H

#include "common.h"

#include <stdlib.h>

#include "word.h"

extern YYYYMMDD today;		/* date as YYYYMMDD */

#define MSG_COUNT ".MSG_COUNT"

/* typedef:  Datastore handle type
** - used to communicate between datastore layer and database layer
** - known to program layer as a void*
*/

typedef struct {
    void   *dbh;		/* database handle from db_open() */
    bool is_swapped;
} dsh_t;

/* typedef:  Datastore value type
** - used to communicate between program layer and datastore layer
*/

typedef struct {
    u_int32_t count[IX_SIZE];	/* spam and ham counts */
    u_int32_t date;
} dsv_t;

#define	spamcount count[IX_SPAM]
#define	goodcount count[IX_GOOD]

/* typedef:  Database value type
** - used to communicate between datastore layer and database layer
*/

#define DS_NOTFOUND (-1)

#define CLAMP_INT_MAX(i) ((int)min(INT_MAX, (i)))

typedef struct {
    void     *data;		/* addr of buffer       */
    u_int32_t leng;		/* number of data bytes */
} dbv_t;

/** Iterate over all elements in data base and call \p hook for each item.
 * \p userdata is passed through to the hook function unaltered.
 */
typedef int ds_foreach_t(word_t *token, dsv_t *data, void *userdata);
extern int ds_foreach(void *, ds_foreach_t *hook, void *userdata);

/** Wrapper for ds_foreach that opens and closes file */
extern int ds_oper(const char *path, dbmode_t open_mode, 
		   ds_foreach_t *hook, void *userdata);

/** Initialize database, open and lock files, etc.
 * params: char * path to database file, char * name of database
 * \return opaque pointer to database handle, which must be saved and
 * passed as the first parameter in all subsequent database function calls. 
 */
/*@only@*/ /*@null@*/
extern void *ds_open(const char *path	/** path to database file */, 
		     const char *name	/** name(s) of data base(s) */,
		     dbmode_t mode	/** open mode, DB_READ or DB_WRITE */);

/** Close file and clean up. */
extern void  ds_close(/*@only@*/ void *, bool nosync  /** Normally false, if true, do not synchronize data. This should not be used in regular operation but only to ease the disk I/O load when the lock operation failed. */);

/** Flush pending writes to disk */
extern void ds_flush(void *);

/** Global initialization */
extern void ds_init(void);

/** Cleanup storage allocation */
extern void ds_cleanup(void);

dsh_t *dsh_init(
    void *dbh,			/* database handle from db_open() */
    bool is_swapped);

void dsh_free(void *vhandle);

/** Retrieve the value associated with a given word in a list. 
 * \return zero if the word does not exist in the database. Front-end
 */
extern int  ds_read  (void *vhandle, const word_t *word, /*@out@*/ dsv_t *val);

/** Retrieve the value associated with a given word in a list. 
 * \return zero if the word does not exist in the database. Implementation
 */
extern int ds_get_dbvalue(void *vhandle, const dbv_t *token, /*@out@*/ dbv_t *val);

/** Delete the key */
extern int  ds_delete(void *vhandle, const word_t *word);

/** Set the value associated with a given word in a list. Front end */
extern int  ds_write (void *vhandle, const word_t *word, dsv_t *val);

/** Set the value associated with a given word in a list. Implementation */
extern int ds_set_dbvalue(void *vhandle, const dbv_t *token, dbv_t *val);

/** Update the value associated with a given word in a list */
extern void ds_updvalues(void *vhandle, const dbv_t *token, const dbv_t *updval);

/** Get the database message count */
extern bool ds_get_msgcounts(void *vhandle, dsv_t *val);

/** set the database message count */
extern void ds_set_msgcounts(void *vhandle, dsv_t *val);

/* Get the current process id */
extern unsigned long ds_handle_pid(void *vhandle);

/* Get the database filename */
extern char *ds_handle_filename(void *vhandle);

/* Locks and unlocks file descriptor */
extern int ds_lock(int fd, int cmd, short int type);

/* Returns version string */
extern const char *ds_version_str(void);

#endif
