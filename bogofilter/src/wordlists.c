/* $Id$ */

#include "common.h"

#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>

#include "bogofilter.h"
#include "datastore.h"
#include "find_home.h"
#include "msgcounts.h"
#include "paths.h"
#include "rand_sleep.h"
#include "wordlists.h"
#include "xmalloc.h"
#include "xstrdup.h"

#define	MIN_SLEEP	0.5e+3		/* 0.5 milliseconds */
#define	MAX_SLEEP	2.0e+6		/* 2.0 seconds */

#ifndef	RAND_MAX			/* SunOS 4.1.X needs this */
#define	RAND_MAX	2147483647	/* May not work on SysV   */
#endif

/* Function Definitions */

void open_wordlists(dbmode_t mode)
{
    wordlist_t *list;
    int retry;

    do {
	ds_init();

	retry = 0;
	for (list = word_lists; list != NULL; list = list->next) {
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
		if (!ds_get_wordlist_version(list->dsh, &val))
		    wordlist_version = 0;
		else
		    wordlist_version = val.count[0];
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

/** Sum up the msgcount for each of the word lists, store the
 * count of bad messages in \a mb and the count of good messages in the
 * data bases in \a mg. */
void compute_msg_counts(void)
{
    u_int32_t s = 0, g = 0;
    wordlist_t* list;

    for(list=word_lists; list != NULL; list=list->next)
    {
	s += list->msgcount[IX_SPAM];
	g += list->msgcount[IX_GOOD];
    }
    set_msg_counts(g, s);
}
