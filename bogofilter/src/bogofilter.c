/* $Id$ */

/*****************************************************************************

NAME:
   bogofilter.c -- detect spam and bogons presented on standard input.

AUTHOR:
   Eric S. Raymond <esr@thyrsus.com>

THEORY:

   Originally implemented as Paul Graham's variant of Bayes filtering,
   as described in 

     "A Plan For Spam", http://www.paulgraham.com/spam.html

   Updated in accordance with Gary Robinson's proposed modifications,
   as described at

    http://radio.weblogs.com/0101454/stories/2002/09/16/spamDetection.html

******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <config.h>
#include "common.h"

#include "bogoconfig.h"
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
    if (Rtable || verbose>=2)
	method->print_stats(fp);
}

rc_t bogofilter()
/* evaluate text for spamicity */
{
    rc_t	status;
    double 	spamicity;
    wordhash_t  *wordhash = wordhash_init();
    long	msgcount = 0;
    token_t	token_type;

    set_list_active_status(true);

    method->initialize();

    if (query)
	query_config();

    /* tokenize input text and save words in a wordhash. */
    while (1) {
	token_type = collect_words(wordhash);
	++msgcount;
	if (token_type == NONE || 
	    token_type == MSG_COUNT_LINE || 
	    ((token_type == FROM) && mbox_mode))
	    break;
    }

    wordhash_sort(wordhash);

    spamicity = method->compute_spamicity(wordhash, NULL);

    status = method->status();

    if (run_type & RUN_UPDATE)		/* Note: don't register if RC_UNSURE */
    {
	if (status == RC_SPAM)
	    register_words(REG_SPAM, wordhash, msgcount);
	if (status == RC_HAM)
	    register_words(REG_GOOD, wordhash, msgcount);
    }

    wordhash_free(wordhash);

    return (token_type == NONE ? status : RC_MORE);
}

/* Done */
