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
    long msgcount[IX_SIZE];	/* count of messages in wordlist. */
    double weight[IX_SIZE];
    bool active;
    bool bad[IX_SIZE];
    int  override;
    bool ignore;
};

extern	wl_t	wl_default;
extern	wl_t	wl_mode   ;

/*@null@*/ 
extern wordlist_t *word_list;

int init_wordlist(/*@out@*/ wordlist_t **list, const char* name, const char* path,
		  double sweight, bool sbad, 
		  double gweight, bool gbad, 
		  int override, bool ignore);

void free_wordlists(void);
int  setup_wordlists(const char* dir, priority_t precedence);

#endif	/* WORDLISTS_CORE_H */
