/* $Id$ */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "system.h"
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include <config.h>
#include "common.h"

#include "bogofilter.h"
#include "datastore.h"
#include "error.h"
#include "paths.h"
#include "wordlists.h"
#include "xmalloc.h"
#include "xstrdup.h"

#define	MIN_SLEEP	1000		/* 1 millisecond */
#define	MAX_SLEEP	1000000l	/* 1 second */

wordlist_t *good_list;
wordlist_t *spam_list;
/*@null@*/ wordlist_t* word_lists=NULL;

/* returns -1 for error, 0 for success */
static int init_wordlist(/*@out@*/ wordlist_t **list, const char* name, const char* path,
			 double weight, bool bad, int override, bool ignore)
{
    wordlist_t *new = (wordlist_t *)xmalloc(sizeof(*new));
    wordlist_t* list_ptr;
    static int listcount;

    *list = new;

    new->dbh=NULL;
    new->filename=xstrdup(name);
    new->filepath=xstrdup(path);
    new->index = ++listcount;
    new->msgcount=0;
    new->override=override;
    new->active=false;
    new->weight=weight;
    new->bad=bad;
    new->ignore=ignore;

    if (! word_lists) {
	word_lists=new;
	new->next=NULL;
	return 0;
    }
    list_ptr=word_lists;

    /* put lists with high override numbers at the front. */
    while(1) {
	if (list_ptr->override < override) {
	    word_lists=new;
	    new->next=list_ptr;
	    break;
        }
        if (! list_ptr->next) {
	    /* end of list */
	    list_ptr->next=new;
	    new->next=NULL;
	    break;
	}
	list_ptr=list_ptr->next;
    }
    return 0;
}

/* returns -1 for error, 0 for success */
int setup_wordlists(const char* dir)
{
    int rc = 0;
    char filepath[PATH_LEN];
    double good_weight = 1.0;
    double bad_weight  = 1.0;

    if (check_directory(dir)) {
	(void)fprintf(stderr, "%s: cannot find bogofilter directory.\n"
		      "You must specify a directory on the command line, in the config file,\n"
		      "or by using the BOGOFILTER_DIR or HOME environment variables.\n"
		      "Program aborting.\n", progname);
	rc = -1;
    }

    if ((build_path(filepath, sizeof(filepath), dir, GOODFILE) < 0) ||
	init_wordlist(&good_list, "good", filepath, good_weight, false, 0, false) != 0)
	rc = -1;

    if ((build_path(filepath, sizeof(filepath), dir, SPAMFILE) < 0) ||
	init_wordlist(&spam_list, "spam", filepath, bad_weight, true, 0, false) != 0)
	rc = -1;

    return rc;
}

void open_wordlists(dbmode_t mode)
{
    wordlist_t *list;

retry:
    for (list = word_lists; list != NULL; list = list->next)
    {
	list->dbh = db_open(list->filepath, list->filename, mode, directory);
	if (list->dbh == NULL) {
	    int err = errno;
	    close_wordlists(); /* unlock and close */
	    switch(err) {
		/* F_SETLK can't obtain lock */
		case EAGAIN:
		case EACCES:
		    {
			struct timeval to;
			to.tv_sec = 0;
			to.tv_usec = MIN_SLEEP + (long) ((MAX_SLEEP-MIN_SLEEP)*rand()/(RAND_MAX+1.0));
			select(0,NULL,NULL,NULL,&to);
		    }
		    goto retry;
		default:
		    fprintf(stderr, "Can't open %s (%s), errno %d, %s\n", list->filename, list->filepath, err, strerror(err));
		    exit(2);
	    } /* switch */
	} /* db_open */
    } /* for */
}

void close_wordlists(void)
{
    wordlist_t *list;

    for ( list = word_lists; list != NULL; list = list->next )
    {
	if (list->dbh) db_close(list->dbh);
	list->dbh = NULL;
    }
}

void free_wordlists(void) {
    wordlist_t *list;

    for ( list = word_lists; list != NULL; list = list->next )
    {
	if (list->filename) free(list->filename);
	if (list->filepath) free(list->filepath);
    }
}

void set_good_weight(double weight)
{
    wordlist_t *list;

    for ( list = word_lists; list != NULL; list = list->next )
    {
	if ( ! list->bad )
	    list->weight = weight;
    }

    return;
}

void set_list_active_status(bool status)
{
    wordlist_t *list;

    for ( list = word_lists; list != NULL; list = list->next )
    {
	list->active = status;
    }

    return;
}

#ifdef COMPILE_DEAD_CODE
/* some sanity checking of lists is needed because we may
   allow users to specify lists eventually and there are
   opportunities to generate divide-by-zero exceptions or
   follow bogus pointers. */
static void sanitycheck_lists(void)
{
    wordlist_t* list=word_lists;
    int listcount=0;

    while(1) {
	if (!list) break;
	if (! list->name) {
	    fprintf(stderr, "A list has no name.\n");
	    exit(2);
	}
	if (list->msgcount==0) {
	    fprintf(stderr, "list %s has zero message count.\n", list->name);
	    exit(2);
	}
	listcount++;
	list=list->next;
    }
    if (0==listcount) {
	fprintf(stderr, "No wordlists available!\n");
	exit(2);
    }
    if (DEBUG_WORDLIST(1))
	fprintf(dbgout, "%d lists look OK.\n", listcount);
}
#endif

/* type - 'g', 's', or 'i'
 * name - 'good', 'spam', etc
 * path - 'goodlist.db'
 * weight - 1,2,...
 * override - 1,2,...
 */

/* returns true for success, false for error */
bool configure_wordlist(const char *val)
{
    bool ok;
    int rc;
    wordlist_t* list = xcalloc(1, sizeof(wordlist_t));
    char* type;
    char* name;
    char* path;
    double weight = 0.0f;
    bool bad = false;
    bool override = false;
    bool ignore = false;

    char *tmp = xstrdup(val);

    type=tmp;
    tmp += strcspn(tmp, ", \t");
    *tmp++ = '\0';
    while (isspace((unsigned char)*tmp)) tmp += 1;

    switch (type[0])
    {
	case 'g':		/* good */
	    break;
	case 's':
	case 'b':		/* spam */
	    bad = true;
	    break;
	default:
	    fprintf( stderr, "Unknown list type - '%c'\n", type[0]);
	    break;
    }

    name=tmp;
    tmp += strcspn(tmp, ", \t");
    *tmp++ = '\0';
    while (isspace(*tmp)) tmp += 1;

    path=tmp;
    tmp += strcspn(tmp, ", \t");
    *tmp++ = '\0';
    while (isspace(*tmp)) tmp += 1;

    weight=atof(tmp);
    tmp += strcspn(tmp, ", \t");
    *tmp++ = '\0';
    while (isspace(*tmp)) tmp += 1;

    override=atoi(tmp);
    tmp += strcspn(tmp, ", \t");
    *tmp++ = '\0';
    while (isspace(*tmp)) tmp += 1;

    if (isdigit(*tmp))
	ignore=atoi(tmp);
    else {
	ignore = false;		/* default is "don't ignore" */
	switch (tolower(*tmp)) {
	case 'n':		/* no */
	case 'f':		/* false */
	    ignore = false;
	    break;
	case 'y':		/* yes */
	case 't':		/* true */
	    ignore = true;
	    break;
	}
    }
    while (isalnum(*tmp))
	tmp++;
    while (isspace(*tmp)) tmp += 1;

    rc = init_wordlist(&list, name, path, weight, bad, override, ignore);
    ok = rc == 0;

    return ok;
}
