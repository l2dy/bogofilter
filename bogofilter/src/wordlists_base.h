/* $Id$ */

/*  constants and declarations for wordlists_base.c */

#ifndef	WORDLISTS_CORE_H
#define	WORDLISTS_CORE_H

#ifndef	DATASTORE_H
typedef void *dsh_t;
#endif

extern	bool	config_setup;

typedef enum e_WL_TYPE {
    WL_REGULAR =	'R',
    WL_IGNORE  =	'I'
} WL_TYPE;

typedef struct wordlist_s wordlist_t;
struct wordlist_s
{
    /*@null@*/ wordlist_t *next;
    int index;
    /*@owned@*/ char *listname;	/* resource name (for debug/verbose messages) */
    /*@owned@*/ char *filepath;	/* resource path (for debug/verbose messages) */
    /*@owned@*/ dsh_t *dsh;	/* datastore handle */
    u_int32_t	msgcount[IX_SIZE];	/* count of messages in wordlist. */
    WL_TYPE	type;		/* 'I' for "ignore" */
    int		override;
};

void init_wordlist(const char* name, const char* path,
		   int override, WL_TYPE type);
void display_wordlists(void);

void free_wordlists(void);

wordlist_t * default_wordlist(void);
int  set_wordlist_dir(const char* dir, priority_t precedence);
  
#endif	/* WORDLISTS_CORE_H */
