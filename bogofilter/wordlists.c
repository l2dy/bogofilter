/* $Id$ */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <config.h>
#include "common.h"

#include "bogofilter.h"
#include "datastore.h"
#include "error.h"
#include "wordlists.h"
#include "xmalloc.h"
#include "xstrdup.h"

wordlist_t good_list;
wordlist_t spam_list;
/*@null@*/ wordlist_t* word_lists=NULL;

void *open_wordlist( const char *name, const char *filepath )
{
    void *dbh;			/* database handle  */

    dbmode_t open_mode = (run_type==RUN_NORMAL) ? DB_READ : DB_WRITE;

    if ( (dbh = db_open(filepath, name, open_mode)) == NULL){
      PRINT_ERROR("Cannot initialize database %s.", name);
      exit(2);
    }

    return dbh;
}

/* returns -1 for error, 0 for success */
static int init_list(/*@out@*/ wordlist_t* list, const char* name, const char* filepath,
	      double weight, bool bad, int override, bool ignore)
{
    wordlist_t* list_index;
    wordlist_t** last_list_ptr;

    list->dbh=open_wordlist(name, filepath);
    if (list->dbh == NULL) return -1;
    list->name=xstrdup(name);
    list->msgcount=0;
    list->override=override;
    list->active=false;
    list->weight=weight;
    list->bad=bad;
    list->ignore=ignore;

    if (! word_lists) {
	word_lists=list;
	list->next=NULL;
	return 0;
    }
    list_index=word_lists;
    last_list_ptr=&word_lists;

    /* put lists with high override numbers at the front. */
    while(1) {
	if (list_index->override < override) {
	    *last_list_ptr=list;
	    list->next=list_index;
	    break;
        }
        if (! list_index->next) {
	    /* end of list */
	    list_index->next=list;
	    list->next=NULL;
	    break;
	}
	list_index=list_index->next;
    }
    return 0;
}

/* build an path to a file given a directory and file name,
 * concatenating dir and file, adding a slash if necessary
 *
 * returns: -1 for overflow
 *	     0 for success
 */
int build_path(char* dest, size_t size, const char* dir, const char* file)
{
    if (strlcpy(dest, dir, size) >= size) return -1;

    if ('/' != dest[strlen(dest)-1]) {
	if (strlcat(dest, "/", size) >= size) return -1; /* RATS: ignore */
    }

    if (strlcat(dest, file, size) >= size) return -1;
    return 0;
}

/* returns -1 for error, 0 for success */
int setup_lists(const char* dir, double good_weight, double bad_weight)
{
    int rc = 0;
    char filepath[PATH_LEN];

    if (build_path(filepath, sizeof(filepath), dir, GOODFILE) < 0) rc = -1;
    if (init_list(&good_list, "good", filepath, good_weight, false, 0, 0)) rc = -1;

    if (build_path(filepath, sizeof(filepath), dir, SPAMFILE) < 0) rc = -1;
    if (init_list(&spam_list, "spam", filepath, bad_weight, true,  0, 0)) rc = -1;

    return rc;
}

void close_lists(void)
{
    wordlist_t* list;
    for ( list = word_lists; list != NULL; list = list->next )
    {
	db_close(list->dbh);
	if (list->name) free(list->name);
    }
}

void set_good_weight(double weight)
{
    wordlist_t* list;
    for ( list = word_lists; list != NULL; list = list->next )
    {
	if ( ! list->bad )
	    list->weight = weight;
    }
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
	if ((list->msgcount==0) && (! list->ignore)) {
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
	fprintf(stderr, "%d lists look OK.\n", listcount);
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
    int override = 0;
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
	case 'i':		/* ignore */
	    ignore = true;
	    break;
	default:
	    fprintf( stderr, "Unknown list type - '%c'\n", type[0]);
	    break;
    }

    name=tmp;
    tmp += strcspn(tmp, ", \t");
    *tmp++ = '\0';
    while (isspace((unsigned char)*tmp)) tmp += 1;

    path=tmp;
    tmp += strcspn(tmp, ", \t");
    *tmp++ = '\0';
    while (isspace((unsigned char)*tmp)) tmp += 1;

    weight=atof(tmp);
    tmp += strcspn(tmp, ", \t");
    *tmp++ = '\0';
    while (isspace((unsigned char)*tmp)) tmp += 1;

    override=atoi(tmp);
    tmp += strcspn(tmp, ", \t");
    *tmp++ = '\0';
    while (isspace((unsigned char)*tmp)) tmp += 1;

    rc = init_list(list, name, path, weight, bad, override, ignore);
    ok = rc == 0;

    return ok;
}
