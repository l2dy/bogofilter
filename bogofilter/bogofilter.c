/* $Id$ */

/*****************************************************************************

NAME:
   bogofilter.c -- detect spam and bogons presented on standard input.

AUTHOR:
   Eric S. Raymond <esr@thyrsus.com>

THEORY:
   This is Paul Graham's variant of Bayes filtering described at 

	http://www.paulgraham.com/spam.html

I do the lexical analysis slightly differently, however.

MOD: (Greg Louis <glouis@dynamicro.on.ca>) This version implements Gary
    Robinson's proposed modifications to the "spamicity" calculation and
    uses his f(w) individual probability calculation.  See

    http://radio.weblogs.com/0101454/stories/2002/09/16/spamDetection.html
    
    Robinson's method does not store "extrema."  Instead it accumulates
    Robinson's P and Q using all words deemed "characteristic," i.e. having
    a deviation (fabs (0.5f - prob)) >= MIN_DEV, currently set to 0.0.

******************************************************************************/

#include <math.h>
#include <float.h> /* has DBL_EPSILON */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "config.h"

#include "common.h"
#include "globals.h"
#include "bogofilter.h"
#include "graham.h"
#include "robinson.h"
#include "datastore.h"
#include "register.h"
#include "rstats.h"
#include "wordhash.h"
#include "wordlists.h"

#include <assert.h>

#define EPS		(100.0 * DBL_EPSILON) /* equality cutoff */

#ifdef	ENABLE_GRAHAM_METHOD
/* constants for the Graham formula */
#define MINIMUM_FREQ	5		/* minimum freq */

#define MAX_PROB	0.99f		/* max probability value used */
#define MIN_PROB	0.01f		/* min probability value used */
#define DEVIATION(n)	fabs((n) - EVEN_ODDS)	/* deviation from average */
#endif

#define GRAHAM_MIN_DEV		0.4f	/* look for characteristic words */
#define ROBINSON_MIN_DEV	0.0f	/* if nonzero, use characteristic words */

#define GRAHAM_SPAM_CUTOFF	0.90f	/* if it's spammier than this... */
#define ROBINSON_SPAM_CUTOFF	0.54f	/* if it's spammier than this... */

void print_bogostats(FILE *fp, double spamicity)
{
#ifdef	ENABLE_GRAHAM_METHOD
    if (algorithm == AL_GRAHAM)
    {
	gra_print_bogostats(fp, spamicity);
    }
#endif

#ifdef	ENABLE_ROBINSON_METHOD
    if (algorithm == AL_ROBINSON)
    {
	rob_print_bogostats(fp, spamicity);
    }
#endif
}

void initialize_constants(void)
{
#ifdef	ENABLE_GRAHAM_METHOD
    if (algorithm == AL_GRAHAM)
    {
	gra_initialize_constants();
    }
#endif

#ifdef	ENABLE_ROBINSON_METHOD
    if (algorithm == AL_ROBINSON)
    {
	rob_initialize_constants();
    }
#endif
}

rc_t bogofilter(double *xss) /*@globals errno@*/
/* evaluate text for spamicity */
{
    rc_t	status;
    double 	spamicity;
    wordhash_t  *wordhash;
    int		wordcount, msgcount;

    good_list.active = spam_list.active = TRUE;

    db_lock_reader_list(word_lists);

    good_list.msgcount = db_getcount(good_list.dbh);
    spam_list.msgcount = db_getcount(spam_list.dbh);

    initialize_constants();

    /* tokenize input text and save words in a wordhash. */
    wordhash = collect_words(&msgcount, &wordcount);

#ifdef	ENABLE_GRAHAM_METHOD
    if (algorithm == AL_GRAHAM)
    {
	/* computes the spamicity of the spam/nonspam indicators. */
	spamicity = gra_bogofilter(wordhash, NULL);
    }
#endif

#ifdef	ENABLE_ROBINSON_METHOD
    if (algorithm == AL_ROBINSON)
    {
	/* computes the spamicity of the spam/nonspam indicators. */
	spamicity = rob_bogofilter(wordhash, NULL);
    }
#endif

    db_lock_release_list(word_lists);

    status = (spamicity > spam_cutoff) ? RC_SPAM : RC_NONSPAM;

    if (xss != NULL)
        *xss = spamicity;

    if (run_type == RUN_UPDATE)
      register_words((status==RC_SPAM) ? REG_SPAM : REG_GOOD, wordhash, msgcount, wordcount);

    wordhash_free(wordhash);

    return status;
}

/* Done */
