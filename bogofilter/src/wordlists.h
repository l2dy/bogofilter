/* $Id$ */

/*  constants and declarations for wordlists */

#ifndef	WORDLISTS_H
#define	WORDLISTS_H

#include "system.h"
#include "datastore.h"

extern const char *aCombined[];
extern size_t	   cCombined;
extern const char *aSeparate[];
extern size_t	   cSeparate;

typedef	char FILEPATH[PATH_LEN];

typedef struct wordlist_s wordlist_t;
struct wordlist_s
{
    /*@null@*/ wordlist_t *next;
    int index;
    /*@owned@*/ char *filename;	/* resource name (for debug/verbose messages) */
    /*@owned@*/ char *filepath;	/* resource path (for debug/verbose messages) */
    /*@owned@*/ dsh_t *dsh;	/* datastore handle */
    long msgcount[IX_SIZE];	/* count of messages in wordlist. */
    double weight[IX_SIZE];
    bool active;
    bool bad[IX_SIZE];
    int  override;
    bool ignore;
};

/*@null@*/ 
extern wordlist_t *word_list;
extern wordlist_t *word_lists;

void incr_wordlist_mode(void);
void set_wordlist_mode(const char *filepath);
size_t build_wordlist_paths(char **filepaths, const char *path);

int  setup_wordlists(const char* dir, priority_t precedence);
bool configure_wordlist(const char *val);

void open_wordlists(dbmode_t);
void close_wordlists(bool);
void free_wordlists(void);

void set_good_weight(double weight);
void set_list_active_status(bool status);

#endif	/* WORDLISTS_H */
