/* $Id$ */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <wordlists.h>
#include <bogofilter.h>
#include <datastore.h>
#include "xmalloc.h"
#include "xstrdup.h"

#define GOODFILE	"goodlist.db"
#define SPAMFILE	"spamlist.db"

wordlist_t good_list;
wordlist_t spam_list;
wordlist_t* word_lists=NULL;

void *open_wordlist( const char *name, const char *filepath )
{
    int	rc;
    void *dbh;			// database handle 

    dbmode_t open_mode = (run_type==RUN_NORMAL) ? DB_READ : DB_WRITE;

    if ( (dbh = db_open(filepath, name, open_mode)) == NULL){
      fprintf(stderr, "bogofilter: Cannot initialize database %s.\n", name);
      exit(2);
    }

    return dbh;
}

/* returns -1 for error, 0 for success */
int init_list(wordlist_t* list, const char* name, const char* filepath, double weight, bool bad, int override, int ignore)
{
    wordlist_t* list_index;
    wordlist_t** last_list_ptr;

    list->dbh=open_wordlist(name, filepath);
    if (list->dbh == NULL) return -1;
    list->name=xstrdup(name);
    list->msgcount=0;
    list->override=override;
    list->active=FALSE;
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

    // put lists with high override numbers at the front.
    while(1) {
	if (list_index->override < override) {
	    *last_list_ptr=list;
	    list->next=list_index;
	    break;
        }
        if (! list_index->next) {
	    // end of list
	    list_index->next=list;
	    list->next=NULL;
	    break;
	}
	list_index=list_index->next;
    }
    return 0;
}

/* returns -1 for error, 0 for success */
int setup_lists(const char* directory)
{
    int rc = 0;
    char filepath[PATH_LEN];

    build_path(filepath, PATH_LEN, directory, GOODFILE);
    if (init_list(&good_list, "good", filepath, GOOD_BIAS, FALSE, 0, 0)) rc = -1;

    build_path(filepath, PATH_LEN, directory, SPAMFILE);
    if (init_list(&spam_list, "spam", filepath, 1, TRUE,  0, 0)) rc = -1;

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

/* some sanity checking of lists is needed because we may
   allow users to specify lists eventually and there are
   opportunities to generate divide-by-zero exceptions or
   follow bogus pointers. */
void sanitycheck_lists()
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
    if (verbose >= 2)
	fprintf(stderr, "%d lists look OK.\n", listcount);
}

// Done
