/* $Id$ */

/*  constants and declarations for wordlists */

#ifndef	WORDLISTS_H
#define	WORDLISTS_H

#include "wordlists_base.h"

extern const char *aCombined[];
extern size_t	   cCombined;
extern const char *aSeparate[];
extern size_t	   cSeparate;

typedef	char FILEPATH[PATH_LEN];

/*@null@*/ 
extern wordlist_t *word_lists;

void incr_wordlist_mode(void);
void set_wordlist_mode(const char *filepath);
size_t build_wordlist_paths(char **filepaths, const char *path);

bool configure_wordlist(const char *val);

void open_wordlists(dbmode_t);
void close_wordlists(bool);

void set_good_weight(double weight);
void set_list_active_status(bool status);

void compute_msg_counts(void);

#endif	/* WORDLISTS_H */
