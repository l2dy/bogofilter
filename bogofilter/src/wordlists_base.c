/* $Id$ */

#include "common.h"

#include "bogohome.h"
#include "find_home.h"
#include "mxcat.h"
#include "paths.h"
#include "wordlists.h"
#include "wordlists_base.h"
#include "xmalloc.h"
#include "xstrdup.h"

bool	config_setup = false;

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

    if (a->filepath != NULL &&
	b->filepath != NULL &&
	strcmp(a->filepath, b->filepath) != 0)
	return false;

    return true;
}

/** Free a wordlist node and return the pointer to the next node */
static wordlist_t *free_wordlistnode(wordlist_t *node)
{
    wordlist_t *next = node->next;

    xfree(node->listname);
    xfree(node->filepath);
    xfree(node->dirname);
    xfree(node->filename);
    xfree(node);

    return next;
}

void init_wordlist(const char* name, const char* path,
		   int override, WL_TYPE type)
{
    wordlist_t *n = (wordlist_t *)xcalloc(1, sizeof(*n));
    wordlist_t *list_ptr;

    /* initialize list node */
    n->listname=xstrdup(name);
    n->dirname =get_directory_from_path(path);
    n->filename=get_file_from_path(path);
    n->filepath=mxcat(n->dirname, DIRSEP_S, n->filename, NULL);
    n->type    =type;
    n->override=override;

    /* now enqueue according to "override" (priority) */
    list_ptr=word_lists;

    if (list_ptr == NULL ||
	list_ptr->override > override) {
	/* prepend to list */
	n->next=word_lists;
	word_lists=n;
	return;
    }

    for ( ; ; list_ptr=list_ptr->next) {
	/* drop duplicates */
	if (is_dup_wordlist(n, list_ptr)) {
	    free_wordlistnode(n);
	    return;
	}

	/* append to list */
	if (list_ptr->next == NULL) {
	    list_ptr->next=n;
	    return;
	}

	/* insert into middle of list */
	if (list_ptr->next->override > override) {
	    n->next=list_ptr->next;
	    list_ptr->next=n;
	    return;
	}
    }
}

wordlist_t *get_default_wordlist(wordlist_t *list)
{
    for (; list != NULL ; list = list->next)
    {
	if (list->type != WL_IGNORE)
	    return list;
    }

    /* not found -> abort */
    fprintf(stderr, "Can't find default wordlist.\n");
    exit(EX_ERROR);
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

    dir = (d != NULL) ? tildeexpand(d) : get_directory(precedence);
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
    xfree(dir);

    return rc;
}

void free_wordlists(wordlist_t *list)
{
    while (list)
    {
	list = free_wordlistnode(list);
    }

    free_bogohome();
}

void display_wordlists(wordlist_t *list, const char *fmt)
{
    for (; list; list = list->next)
    {
	printf(fmt, "wordlist");
	printf("%s,%s,%s,%d\n",
		(list->type == WL_REGULAR) ? "r" : "i",
		list->listname,
		list->filepath,
		list->override);
    }
}
