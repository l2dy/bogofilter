/* $Id$ */

#include "common.h"

#include "bogohome.h"
#include "find_home.h"
#include "paths.h"
#include "wordlists.h"
#include "wordlists_base.h"
#include "xmalloc.h"
#include "xstrdup.h"

bool	config_setup = false;

static wordlist_t *free_wordlist(wordlist_t *list);
static bool	    dup_wordlist(wordlist_t *a, wordlist_t *b);

/* Default wordlist mode is now wordlist.db -
   a single wordlist containing ham and spam tokens */

/*@null@*/ wordlist_t* word_lists=NULL;

/* returns -1 for error, 0 for success */
void init_wordlist(const char* name, const char* path,
		   int override, WL_TYPE type)
{
    wordlist_t *n = (wordlist_t *)xcalloc(1, sizeof(*n));
    wordlist_t *list_ptr;
    static int listcount;

    n->dsh=NULL;
    n->listname=xstrdup(name);
    n->filepath=xstrdup(path);
    n->index   =++listcount;
    n->type    =type;
    n->next    =NULL;
    n->override=override;

    list_ptr=word_lists;

    if (list_ptr == NULL ||
	list_ptr->override > override) {
	n->next=word_lists;
	word_lists=n;
	return;
    }

    while(1) {
	if (dup_wordlist(n, list_ptr)) {
	    free_wordlist(n);
	    return;
	}

        if (list_ptr->next == NULL) {
	    /* end of list */
	    list_ptr->next=n;
	    return;
	}

	if (list_ptr->next->override > override) {
	    n->next=list_ptr->next;
	    list_ptr->next=n;
	    return;
        }

	list_ptr=list_ptr->next;
    }
}

static bool dup_wordlist(wordlist_t *a, wordlist_t *b)
{
    if (a->type != b->type)
	return false;

    if (a->override!= b->override)
	return false;

    if (strcmp(a->listname, b->listname) != 0)
	return false;

    if (strcmp(a->filepath, b->filepath) != 0)
	return false;

    return true;
}


/* Set default wordlist for registering messages, finding robx, etc */

wordlist_t *default_wordlist(void)
{
    wordlist_t *list;
    for ( list = word_lists; list != NULL; list = list->next )
    {
 	if (list->type != WL_IGNORE)
 	    return list;
    }
    return NULL;
}

/* setup_wordlists()
   returns: -1 for error, 0 for success
   **
   ** precedence: (high to low)
   **
   **	command line
   **	$BOGOFILTER_DIR
   **	user config file
   **	site config file
   **	$HOME
   */

int set_wordlist_dir(const char* d, priority_t precedence)
{
    int rc = 0;
    char *dir;
    static priority_t saved_precedence = PR_NONE;

    if (DEBUG_WORDLIST(2))
	fprintf(dbgout, "p: %d, s: %d\n", (int) precedence, (int) saved_precedence);

    if (precedence < saved_precedence)
	return rc;

    dir = (d != NULL) ? tildeexpand(d, true) : get_directory(precedence);
    if (dir == NULL)
	return -1;

    if (DEBUG_WORDLIST(2))
	fprintf(dbgout, "d: %s\n", dir);

    saved_precedence = precedence;

    if (!check_directory(dir)) {
	(void)fprintf(stderr, "%s: cannot find bogofilter directory.\n"
		      "You must specify a directory on the command line, in the config file,\n"
#ifndef __riscos__
		      "or by using the BOGOFILTER_DIR or HOME environment variables.\n"
#else
		      "or by ensuring that <Bogofilter$Dir> is set correctly.\n"
#endif
		      "Program aborting.\n", progname);
	rc = -1;
    }

    set_bogohome(dir);

    return rc;
}

static wordlist_t *free_wordlist(wordlist_t *list)
{
    wordlist_t *next = list->next;

    xfree(list->listname);
    xfree(list->filepath);
    xfree(list);

    return next;
}

void free_wordlists(void)
{
    wordlist_t *list = word_lists;

    while ( list != NULL )
    {
	list = free_wordlist(list);
    }

    word_lists = NULL;

    free_bogohome();
}
