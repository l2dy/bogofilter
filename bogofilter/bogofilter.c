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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <config.h>
#include "common.h"

#include "bogofilter.h"
#include "method.h"
#include "datastore.h"
#include "collect.h"
#include "register.h"

void initialize_constants()
{
    method->initialize();
}

void print_stats(FILE *fp)
{
    method->print_stats(fp);
}

rc_t bogofilter(double *xss) /*@globals errno@*/
/* evaluate text for spamicity */
{
    rc_t	status;
    double 	spamicity;
    wordhash_t  *wordhash;
    long	wordcount, msgcount = 0;
    bool	cont;

    good_list.active = spam_list.active = true;

    db_lock_reader_list(word_lists);

    good_list.msgcount = db_getcount(good_list.dbh);
    spam_list.msgcount = db_getcount(spam_list.dbh);

    method->initialize();

    /* tokenize input text and save words in a wordhash. */
    do {
	collect_words(&wordhash, &wordcount, &cont);
	++msgcount;
    } while(cont);

    spamicity = method->compute_spamicity(wordhash, NULL);

    db_lock_release_list(word_lists);

    status = method->status();

    if (xss != NULL)
        *xss = spamicity;

    if (run_type == RUN_UPDATE)		/* Note: don't register if RC_UNSURE */
    {
	if (status == RC_SPAM)
	    register_words(REG_SPAM, wordhash, msgcount, wordcount);
	if (status == RC_HAM)
	    register_words(REG_GOOD, wordhash, msgcount, wordcount);
    }

    wordhash_free(wordhash);

    return status;
}

/* Done */
