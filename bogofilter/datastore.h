/* 
 API for bogofilter datastore.
 The idea here is to make bogofilter independent of the database system
 used to store words. The interface specified by this file determines
 the entire interaction between bogofilter and the database
 Writing a new database backend merely requires the implementation of the interface
 
 Author: Gyepi Sam <gyepi@praxis-sw.com>
 
*/

#include <wordlists.h>

#ifndef DATASTORE_H_GUARD
#define DATASTORE_H_GUARD

/*
Initialize database, open files, etc.
params: char * path to database file, char * name of database
returns: opaque pointer to database handle, which must be saved and
         passed as the first parameter in all subsequent database function calls. 
*/
void *db_open(char *, char *);


/* Close files and clean up. */
void  db_close(void *);


/* Flush pending writes to disk */
void db_flush(void *);


/*
Increments count for given word.
Note: negative results are set to zero, 
*/
void db_increment(void *, char *, long);


/*
Decrement count for a given word, if it exists in the datastore.
Note: negative results are set to zero, 
*/
void db_decrement(void *, char *, long);


/*
Retrieve the value associated with a given word in a list
Returns zero if the word does not exist in the database.
*/
long db_getvalue(void *, char *);


/* Set the value associated with a given word in a list */
void db_setvalue(void *, char *, long);

/* Get the database message count */
long db_getcount(void*);


/* set the database message count */
void db_setcount(void*, long);


/*
Acquire a reader lock on database.
Caller is blocked until the lock is granted.
Multiple readers can have simulatenous locks.
When a writer lock exists, reader lock requests are blocked
until the lock becomes available.
*/

void db_lock_reader(void *);


/*
Acquire a writer lock on database.
Caller is blocked until the lock is granted.
Only one writer lock can exist at any time.
Writer lock requests are blocked until all reader locks are released.
*/
void db_lock_writer(void *);


/* Release acquired lock */
void db_lock_release(void *);



/*
Acquires read locks on multiple databases.
*/
void db_lock_reader_list(wordlist_t *list);


/*
Acquires write locks on multiple database.
*/
void db_lock_writer_list(wordlist_t *list);


/*
Releases acquired locks on multiple databases
*/
void db_lock_release_list(wordlist_t *list);

#endif


