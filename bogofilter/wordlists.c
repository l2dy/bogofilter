/* $Id$ */
/*
 * */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <wordlists.h>
#include <bogofilter.h>
#include <datastore.h>

#define GOODFILE	"goodlist.db"
#define SPAMFILE	"spamlist.db"

wordlist_t good_list;
wordlist_t spam_list;
wordlist_t* first_list=NULL;

void init_list(wordlist_t* list, char* name, double weight, bool bad, int override, int ignore)
{
    wordlist_t* list_index;
    wordlist_t** last_list_ptr;

    list->name=strdup(name);
    list->dbh=NULL;
    list->msgcount=0;
    list->file=NULL;
    list->override=override;
    list->weight=weight;
    list->bad=bad;
    list->ignore=ignore;

    if (! first_list) {
	first_list=list;
	list->next=NULL;
	return;
    }
    list_index=first_list;
    last_list_ptr=&first_list;

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
    }
}

void setup_lists()
{
    init_list(&good_list, "good", GOOD_BIAS, FALSE, 0, 0);
    init_list(&spam_list, "spam",         1, TRUE,  0, 0);
}

void close_lists()
{
    wordlist_t* list;
    for ( list = first_list; list != NULL; list = list->next )
    {
	db_close(list->dbh);
    }
}

/* some sanity checking of lists is needed because we may
   allow users to specify lists eventually and there are
   opportunities to generate divide-by-zero exceptions or
   follow bogus pointers. */
void sanitycheck_lists()
{
    wordlist_t* list=first_list;
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
	printf("%d lists look OK.\n", listcount);
}

// Done
