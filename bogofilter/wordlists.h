/* $Id$ */
/* 
 * */

/*  constants and declarations for wordlists */

#ifndef	HAVE_WORDLISTS_H
#define	HAVE_WORDLISTS_H

typedef enum bool { FALSE = 0, TRUE = 1 } bool;

typedef struct wordlist_s wordlist_t;
struct wordlist_s
{
    wordlist_t *next;
    char *name;			// resource name (for debug/verbose messages)
    void *dbh;			// database handle 
    unsigned long msgcount;	// count of messages in wordlist.
    char *file;			// associated  file
    double weight;
    bool bad;
    int override;
    int ignore;
};

extern wordlist_t *first_list;
extern wordlist_t good_list, spam_list;

extern void setup_lists();
void close_lists();
void *open_wordlist( wordlist_t *list, char *directory, char *filename );

#endif	/* HAVE_WORDLISTS_H */
