/* $Id$ */

/*****************************************************************************

NAME:
   method.c -- define an abstract super class for graham, robinson, and fisher.

   The algorithm class hierarchy looks like:

	method
	|
	+--graham
	|
	+--robinson
	    |
	    +--fisher

******************************************************************************/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <config.h>
#include "common.h"

#include "bogofilter.h"
#include "method.h"
#include "wordlists.h"

/*
** Define a struct so stats can be saved for printing.
*/

stats_t  *mth_stats;

extern double min_dev;

void mth_initialize(void *s, int _max_repeats, double _min_dev, double _spam_cutoff, double _good_weight)
{
    mth_stats = (stats_t *) s;
    max_repeats = _max_repeats;
    if (fabs(min_dev) < EPS)
	min_dev = _min_dev;
    if (spam_cutoff < EPS)
	spam_cutoff = _spam_cutoff;
    set_good_weight( _good_weight );
}

double mth_spamicity(void)
{
    return mth_stats->spamicity;
}

rc_t mth_status(void)
{
    rc_t status = ( mth_stats->spamicity >= spam_cutoff ) ? RC_SPAM : RC_HAM;
    return status;
}
