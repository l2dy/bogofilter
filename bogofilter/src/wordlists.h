/* $Id$ */

/*  constants and declarations for wordlists */

#ifndef	WORDLISTS_H
#define	WORDLISTS_H

#include "bftypes.h"
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
bool build_wordlist_path(char *filepath, size_t size, const char *path);

void open_wordlists(dbmode_t);
void close_wordlists(bool);

void set_list_active_status(bool status);

void compute_msg_counts(u_int32_t *, u_int32_t *);

#endif	/* WORDLISTS_H */
