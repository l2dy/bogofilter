/* $Id$ */

/*  constants and declarations for wordlists */

#ifndef	HAVE_WORDLISTS_H
#define	HAVE_WORDLISTS_H

#include "common.h"

typedef struct wordlist_s wordlist_t;
struct wordlist_s
{
    wordlist_t *next;
    char *name;			/* resource name (for debug/verbose messages) */
    void *dbh;			/* database handle */
    long msgcount;		/* count of messages in wordlist. */
    double weight;
    bool active;
    bool bad;
    int  override;
    bool ignore;
};

extern wordlist_t *word_lists;
extern wordlist_t good_list, spam_list;

int setup_lists(const char* directory, double, double);
bool configure_wordlist(const char *val);

void *open_wordlist( const char *name, const char *filepath);
void close_lists(void);

#endif	/* HAVE_WORDLISTS_H */
