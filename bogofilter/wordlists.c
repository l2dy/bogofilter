/* $Id$ */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <wordlists.h>
#include <bogofilter.h>
#include <datastore.h>
#include "globals.h"
#include "xmalloc.h"
#include "xstrdup.h"

wordlist_t good_list;
wordlist_t spam_list;
wordlist_t* word_lists=NULL;

void *open_wordlist( const char *name, const char *filepath )
{
    void *dbh;			// database handle 

    dbmode_t open_mode = (run_type==RUN_NORMAL) ? DB_READ : DB_WRITE;

    if ( (dbh = db_open(filepath, name, open_mode)) == NULL){
      fprintf(stderr, "%s: Cannot initialize database %s.\n", progname, name);
      exit(2);
    }

    return dbh;
}

/* returns -1 for error, 0 for success */
int init_list(wordlist_t* list, const char* name, const char* filepath,
	      double weight, bool bad, int override, bool ignore)
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

/* build an absolute path to a file given a directory and file name
 */
void build_path(char* dest, int size, const char* dir, const char* file)
{
    int path_left=size-1;

    strncpy(dest, dir, path_left);
    path_left -= strlen(dir);
    if (path_left <= 0) return;

    if ('/' != dest[strlen(dest)-1]) {
	strcat(dest, "/");
	path_left--;
	if (path_left <= 0) return;
    }

    strncat(dest, file, path_left);
}

/* returns -1 for error, 0 for success */
int setup_lists(const char* directory, double good_weight, double bad_weight)
{
    int rc = 0;
    char filepath[PATH_LEN];

    build_path(filepath, PATH_LEN, directory, GOODFILE);
    if (init_list(&good_list, "good", filepath, good_weight, FALSE, 0, 0)) rc = -1;

    build_path(filepath, PATH_LEN, directory, SPAMFILE);
    if (init_list(&spam_list, "spam", filepath, bad_weight, TRUE,  0, 0)) rc = -1;

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
    if (DEBUG_WORDLIST(1))
	fprintf(stderr, "%d lists look OK.\n", listcount);
}

// type - 'g', 's', or 'i'
// name - 'good', 'spam', etc
// path - 'goodlist.db'
// weight - 1,2,...
// override - 1,2,...

bool configure_wordlist(const char *val)
{
    bool ok;
    int rc;
    wordlist_t* list = xcalloc(1, sizeof(wordlist_t));
    char* type;
    char* name;
    char* path;
    double weight = 0.0f;
    bool bad = FALSE;
    int override = 0;
    bool ignore = FALSE;

    char *tmp = xstrdup(val);
	
    type=tmp;
    tmp += strcspn(tmp, ", \t");
    *tmp++ = '\0';
    while (isspace(*tmp)) tmp += 1;

    switch (type[0])
    {
    case 'g':		// good
    {
	break;
    }
    case 's':
    case 'b':		// spam
    {
	bad = TRUE;
	break;
    }
    case 'i':		// ignore
    {
	ignore = TRUE;
	break;
    }
    default:
    {
	fprintf( stderr, "Unknown list type - '%c'\n", type[0]);
	break;
    }
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

    rc = init_list(list, name, path, weight, bad, override, ignore);
    ok = rc == 0;

    return ok;
}

// Done
