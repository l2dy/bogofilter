/* $Id$ */

/*  constants and declarations for wordlists */

#ifndef	WORDLISTS_H
#define	WORDLISTS_H

typedef struct wordlist_s wordlist_t;
struct wordlist_s
{
    /*@null@*/ wordlist_t *next;
    int index;
    /*@owned@*/ char *filename;	/* resource name (for debug/verbose messages) */
    /*@owned@*/ char *filepath;	/* resource path (for debug/verbose messages) */
    /*@owned@*/ void *dbh;	/* database handle */
    long msgcount;		/* count of messages in wordlist. */
    double weight;
    bool active;
    bool bad;
    int  override;
    bool ignore;
};

/*@null@*/ extern wordlist_t *word_lists;
extern wordlist_t *good_list, *spam_list;

int setup_wordlists(const char* dir);
bool configure_wordlist(const char *val);

void open_wordlists(dbmode_t);
void close_wordlists(bool);
void free_wordlists(void);

void set_good_weight(double weight);
void set_list_active_status(bool status);

#endif	/* WORDLISTS_H */
