/* $Id$ */

#include "common.h"

#include "paths.h"
#include "wordlists.h"
#include "wordlists_base.h"
#include "xmalloc.h"
#include "xstrdup.h"

/* Default wordlist mode is now wordlist.db -
   a single wordlist containing ham and spam tokens */

wordlist_t *word_list;
/*@null@*/ wordlist_t* word_lists=NULL;
char *bogohome=NULL;

/* returns -1 for error, 0 for success */
int init_wordlist(/*@out@*/ wordlist_t **list, const char* name, const char* path,
			 double sweight, bool sbad, 
			 double gweight, bool gbad, 
			 int override, bool ignore)
{
    wordlist_t *n = (wordlist_t *)xcalloc(1, sizeof(*n));
    wordlist_t *list_ptr;
    static int listcount;

    *list = n;

    n->dsh=NULL;
    n->filename=xstrdup(name);
    n->filepath=xstrdup(path);
    n->index = ++listcount;
    n->override=override;
    n->active=false;
    n->weight[IX_SPAM]=sweight;
    n->weight[IX_GOOD]=gweight;
    n->bad[IX_SPAM]=sbad;
    n->bad[IX_GOOD]=gbad;
    n->ignore=ignore;

    if (word_lists == NULL) {
	word_lists=n;
	n->next=NULL;
	return 0;
    }
    list_ptr=word_lists;

    /* put lists with high override numbers at the front. */
    while(1) {
	if (list_ptr->override < override) {
	    word_lists=n;
	    n->next=list_ptr;
	    break;
        }

        if (list_ptr->next == NULL) {
	    /* end of list */
	    list_ptr->next=n;
	    n->next=NULL;
	    break;
	}
	list_ptr=list_ptr->next;
    }
    return 0;
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

int setup_wordlists(const char* d, priority_t precedence)
{
    int rc = 0;
    char *dir;
    double good_weight = 1.0;
    double bad_weight  = 1.0;
    static priority_t saved_precedence = PR_NONE;

    if (DEBUG_WORDLIST(2))
	fprintf(dbgout, "p: %d, s: %d\n", (int) precedence, (int) saved_precedence);

    if (precedence < saved_precedence)
	return rc;

    dir = (d != NULL) ? xstrdup(d) : get_directory(precedence);
    if (dir == NULL)
	return -1;

    if (DEBUG_WORDLIST(2))
	fprintf(dbgout, "d: %s\n", dir);

    if (saved_precedence != precedence)
	free_wordlists();

    saved_precedence = precedence;

    if (!check_directory(dir)) {
#ifndef __riscos__
	const char var[] = "using the BOGOFILTER_DIR or HOME environment variables.";
#else
	const char var[] = "ensuring that <Bogofilter$Dir> is set correctly.";
#endif
	(void)fprintf(stderr, "%s: cannot find bogofilter directory.\n"
		      "You must specify a directory on the command line, in the config file,\n"
		      "or by %s\nProgram aborting.\n", progname, var);
	rc = -1;
    }

    if (init_wordlist(&word_list, "word", dir, bad_weight, true, good_weight, false, 0, false) != 0)
	rc = -1;

    xfree(bogohome);
    bogohome = dir;

    return rc;
}

static wordlist_t *free_wordlist(wordlist_t *list)
{
    wordlist_t *next = list->next;

    xfree(list->filename);
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

    xfree(bogohome);
    bogohome = NULL;
}

