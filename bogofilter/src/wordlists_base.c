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

/** priority queue of wordlists, ordered by their "override" parameter */
/*@null@*/ wordlist_t* word_lists=NULL;

/** Check if wordlist nodes \a a and \a b have the same type, override,
 * listname and filepath, \returns true if these parameters match in
 * either list node. */
static bool is_dup_wordlist(wordlist_t *a, wordlist_t *b)
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

void init_wordlist(const char* name, const char* path,
		   int override, WL_TYPE type)
{
    wordlist_t *n = (wordlist_t *)xcalloc(1, sizeof(*n));
    wordlist_t *list_ptr;

    /* initialize list node */
    n->dsh     =NULL;
    n->listname=xstrdup(name);
    n->filepath=xstrdup(path);
    n->type    =type;
    n->next    =NULL;
    n->override=override;

    /* rest of this code: enqueue in the right place */

    /* initialize iterator */
    list_ptr=word_lists;

    if (list_ptr == NULL ||
	list_ptr->override > override) {
	/* prepend to list */
	n->next=word_lists;
	word_lists=n;
	return;
    }

    while(1) {
	if (is_dup_wordlist(n, list_ptr)) {
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

wordlist_t *default_wordlist(void)
{
    wordlist_t *list;
    for ( list = word_lists; list != NULL; list = list->next )
    {
 	if (list->type != WL_IGNORE)
 	    return list;
    }
    fprintf(stderr, "Can't find default wordlist.\n");
    exit(EX_ERROR);
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

void display_wordlists(const char *fmt)
{
    wordlist_t *list;

    for ( list = word_lists; list != NULL ; list = list->next )
    {
	fprintf(stdout, fmt, "wordlist");
	fprintf(stdout, "%s,%s,%s,%d\n",
		(list->type == WL_REGULAR) ? "r" : "i",
		list->listname,
		list->filepath,
		list->override);
    }
}
