/* $Id$ */

/*  constants and declarations for wordlists */

#ifndef	WORDLISTS_H
#define	WORDLISTS_H

#include "system.h"

typedef enum sh_e { SPAM, GOOD } sh_t;

#define	spamcount count[SPAM]
#define	goodcount count[GOOD]

extern const char *aCombined[];
extern size_t	   cCombined;
extern const char *aSeparate[];
extern size_t	   cSeparate;

typedef struct wordlist_s wordlist_t;
struct wordlist_s
{
    /*@null@*/ wordlist_t *next;
    int index;
    /*@owned@*/ char *filename;	/* resource name (for debug/verbose messages) */
    /*@owned@*/ char *filepath;	/* resource path (for debug/verbose messages) */
    /*@owned@*/ void *dbh;	/* database handle */
    long msgcount[2];		/* count of messages in wordlist. */
    double weight[2];
    bool active;
    bool bad[2];
    int  override;
    bool ignore;
};

/*@null@*/ 
extern wordlist_t *word_list;
extern wordlist_t *word_lists;

int setup_wordlists(const char* dir, priority_t precedence);
bool configure_wordlist(const char *val);

void open_wordlists(dbmode_t);
void close_wordlists(bool);
void free_wordlists(void);

void set_good_weight(double weight);
void set_list_active_status(bool status);

#endif	/* WORDLISTS_H */
