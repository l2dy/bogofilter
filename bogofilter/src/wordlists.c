/* $Id$ */

#include "common.h"

#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>

#include "bogofilter.h"
#include "datastore.h"
#include "find_home.h"
#include "paths.h"
#include "wordlists.h"
#include "xmalloc.h"
#include "xstrdup.h"

#define	MIN_SLEEP	0.5e+3		/* 0.5 milliseconds */
#define	MAX_SLEEP	2.0e+6		/* 2.0 seconds */

#ifndef	RAND_MAX			/* SunOS 4.1.X needs this */
#define	RAND_MAX	2147483647	/* May not work on SysV   */
#endif

/* Function Prototypes */

static void rand_sleep(double min, double max);

/* Function Definitions */

static void rand_sleep(double min, double max)
{
    static bool need_init = true;
    long delay;

    if (need_init) {
	struct timeval timeval;
	need_init = false;
	gettimeofday(&timeval, NULL);
	srand((uint)timeval.tv_usec); /* RATS: ignore - this is safe enough */
    }
    delay = (int)(min + ((max-min)*rand()/(RAND_MAX+1.0)));
    bf_sleep(delay);
}

void open_wordlists(dbmode_t mode)
{
    wordlist_t *list;
    int retry;

    do {
	ds_init();

	retry = 0;
	for (list = word_lists; list != NULL; list = list->next) {
	    if (db_cachesize < 4)
		db_cachesize = 4;
	    list->dsh = ds_open(list->filepath, WORDLIST, mode);
	    if (list->dsh == NULL) {
		int err = errno;
		close_wordlists(true); /* unlock and close */
		switch(err) {
		    /* F_SETLK can't obtain lock */
		    case EAGAIN:
		    /* case EACCES: */
			rand_sleep(MIN_SLEEP, MAX_SLEEP);
			retry = 1;
			break;
		    default:
			if (query)	/* If "-Q", failure is OK */
			    return;
			fprintf(stderr,
				"Can't open file '%s' in directory '%s'.\n",
				WORDLIST, list->filepath);
			if (err != 0)
			    fprintf(stderr,
				    "error #%d - %s.\n", err, strerror(err));
			if (err == ENOENT)
			    fprintf(stderr, 
				    "\n"
				    "Remember to register some spam and ham messages before you\n"
				    "use bogofilter to evaluate mail for its probable spam status!\n");
			if (err == EINVAL)
			    fprintf(stderr,
				    "\n"
				    "Make sure that the BerkeleyDB version this program is linked against\n"
				    "can handle the format of the data base file (after updates in particular)\n"
				    "and that your NFS locking, if applicable, works.\n");
			exit(EX_ERROR);
		} /* switch */
	    } else { /* ds_open */
		dsv_t val;
		ds_get_msgcounts(list->dsh, &val);
		list->msgcount[IX_GOOD] = val.goodcount;
		list->msgcount[IX_SPAM] = val.spamcount;
	    } /* ds_open */
	} /* for */
    } while(retry);
}

/** close all open word lists */
void close_wordlists(bool nosync /** Normally false, if true, do not synchronize data. This should not be used in regular operation but only to ease the disk I/O load when the lock operation failed. */)
{
    wordlist_t *list;

    for ( list = word_lists; list != NULL; list = list->next )
    {
	if (list->dsh) ds_close(list->dsh, nosync);
	list->dsh = NULL;
    }

    ds_cleanup();
}

bool build_wordlist_path(char *filepath, size_t size, const char *path)
{
    bool ok = build_path(filepath, size, path, WORDLIST);
    return ok;
}

void set_good_weight(double weight)
{
    wordlist_t *list;

    for ( list = word_lists; list != NULL; list = list->next )
    {
	list->weight[IX_GOOD] = weight;
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
	    exit(EX_ERROR);
	}
	if (list->msgcount==0) {
	    fprintf(stderr, "list %s has zero message count.\n", list->name);
	    exit(EX_ERROR);
	}
	listcount++;
	list=list->next;
    }
    if (0==listcount) {
	fprintf(stderr, "No wordlists available!\n");
	exit(EX_ERROR);
    }
    if (DEBUG_WORDLIST(1))
	fprintf(dbgout, "%d lists look OK.\n", listcount);
}
#endif

static char *spanword(char *p)
{
    p += strcspn(p, ", \t");		/* skip to end of word */
    *p++ = '\0';
    while (isspace((unsigned char)*p)) 	/* skip trailing whitespace */
	p += 1;
    return p;
}

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
    double sweight, gweight;
    bool bad = false;
    int  override;
    bool ignore = false;

    char *tmp = xstrdup(val);

    type=tmp;			/* save wordlist type (good/spam) */
    tmp = spanword(tmp);

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

    name=tmp;			/* name of wordlist */
    tmp = spanword(tmp);

    path=tildeexpand(tmp, true);/* path to wordlist */
    tmp = spanword(tmp);

    sweight=atof(tmp);
    tmp = spanword(tmp);

    gweight=atof(tmp);
    tmp = spanword(tmp);

    override=atoi(tmp);
    tmp = spanword(tmp);

    if (isdigit((unsigned char)*tmp))
	ignore=atoi(tmp);
    else {
	ignore = false;		/* default is "don't ignore" */
	switch (tolower((unsigned char)*tmp)) {
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
    tmp = spanword(tmp);

    rc = init_wordlist(&list, name, path, sweight, bad, gweight, false, override, ignore);
    ok = rc == 0;

    xfree(path);

    return ok;
}

void compute_msg_counts(void)
{
    wordlist_t* list;

    for(list=word_lists; list != NULL; list=list->next)
    {
	msgs_bad  += list->msgcount[IX_SPAM];
	msgs_good += list->msgcount[IX_GOOD];
    }
}
