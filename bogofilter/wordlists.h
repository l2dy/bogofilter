/* $Id$ */
/* 
 * $Log$
 * Revision 1.7  2002/10/04 11:58:46  relson
 * Removed obsolete "file" field from wordlist_t.
 * Cleaned up list name, directory, and filename code in open_wordlist().
 * Changed parameters to "const char *" for open_wordlist(), dbh_init(), and db_open().
 *
 * Revision 1.6  2002/10/04 04:34:11  relson
 * Changed "char *" parameters of setup_lists() and init_lists() to "const char *".
 *
 * Revision 1.5  2002/10/04 01:42:36  m-a
 * Cleanup, fixing memory leaks, adding error checking. TODO: let callers (main.c) also check for error return.
 *
 * Revision 1.4  2002/10/04 01:35:06  gyepi
 * Integration of wordlists with datastore and bogofilter code.
 * David Relson did most of the work. I just tweaked the locking code
 * and made a few minor changes.
 *
 * Revision 1.3  2002/10/04 00:39:57  m-a
 * First set of type fixes.
 *
 * Revision 1.2  2002/10/02 17:14:54  relson
 * main.c now calls setup_lists() for initializing the wordlist structures, including the opening of the wordlist.db files.
 * setup_list() takes a directory name as its argument and passes it to init_list(), which calls open_wordlist() for the actual open.
 *
 * */

/*  constants and declarations for wordlists */

#ifndef	HAVE_WORDLISTS_H
#define	HAVE_WORDLISTS_H

#include "common.h"

typedef struct wordlist_s wordlist_t;
struct wordlist_s
{
    wordlist_t *next;
    char *name;			// resource name (for debug/verbose messages)
    void *dbh;			// database handle 
    long msgcount;		// count of messages in wordlist.
    double weight;
    bool active;
    bool bad;
    int override;
    int ignore;
};

extern wordlist_t *word_lists;
extern wordlist_t good_list, spam_list;

int setup_lists(const char *directory);
void close_lists(void);
void *open_wordlist( const char *name, const char *directory, const char *filename );

#endif	/* HAVE_WORDLISTS_H */
