/* $Id$ */

/*  constants and declarations for wordlists_base.c */

#ifndef	WORDLISTS_CORE_H
#define	WORDLISTS_CORE_H

#ifndef	DATASTORE_H
typedef void *dsh_t;
#endif

typedef struct wordlist_s wordlist_t;
struct wordlist_s
{
    /*@null@*/ wordlist_t *next;
    int index;
    /*@owned@*/ char *filename;	/* resource name (for debug/verbose messages) */
    /*@owned@*/ char *filepath;	/* resource path (for debug/verbose messages) */
    /*@owned@*/ dsh_t *dsh;	/* datastore handle */
    u_int32_t	msgcount[IX_SIZE];	/* count of messages in wordlist. */
    bool	bad[IX_SIZE];
    int		override;
};

/*@null@*/ 
extern wordlist_t *word_list;

int init_wordlist(/*@out@*/ wordlist_t **list, const char* name, const char* path,
		  bool sbad, bool gbad, int override);

void free_wordlists(void);
void set_default_wordlist(void);
int  setup_wordlists(const char* dir, priority_t precedence);

#endif	/* WORDLISTS_CORE_H */
