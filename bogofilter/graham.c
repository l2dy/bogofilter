/* $Id$ */

/*****************************************************************************

NAME:
   robinson.c -- code for implementing robinson algorithm for computing spamicity.

MOD: (Greg Louis <glouis@dynamicro.on.ca>) This version implements Gary
    Robinson's proposed modifications to the "spamicity" calculation and
    uses his f(w) individual probability calculation.  See

    http://radio.weblogs.com/0101454/stories/2002/09/16/spamDetection.html
    
    Robinson's method does not store "extrema."  Instead it accumulates
    Robinson's P and Q using all words deemed "characteristic," i.e. having
    a deviation (fabs (0.5f - prob)) >= MIN_DEV, currently set to 0.0.

******************************************************************************/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "config.h"

#include "bogofilter.h"
#include "common.h"
#include "datastore.h"
#include "globals.h"
#include "graham.h"
#include "lexer.h"
#include "method.h"
#include "wordhash.h"

method_t graham_method = {
    gra_initialize_constants, 		/* alg_initialize_constants *initialize_constants; */
    gra_bogofilter,	 		/* alg_compute_spamicity *compute_spamicity; */
    gra_print_bogostats, 		/* alg_print_bogostats *print_stats; */
    gra_cleanup 			/* alg_free *cleanup; */
} ;

/* constants for the Graham formula */
#define KEEPERS		15		/* how many extrema to keep */
#define MINIMUM_FREQ	5		/* minimum freq */

#define MAX_PROB	0.99f		/* max probability value used */
#define MIN_PROB	0.01f		/* min probability value used */

#define GRAHAM_MIN_DEV		0.4f	/* look for characteristic words */
#define GRAHAM_SPAM_CUTOFF	0.90f	/* if it's spammier than this... */
#define GRAHAM_MAX_REPEATS	4	/* cap on word frequency per message */

extern double min_dev;

typedef struct 
{
    /*@unique@*/ 
    char        key[MAXTOKENLEN+1];
    double      prob; /* WARNING: DBL_EPSILON IN USE, DON'T CHANGE */
} discrim_t;

struct bogostat_s
{
    discrim_t extrema[KEEPERS];
};

#define SIZEOF(array)	((size_t)(sizeof(array)/sizeof(array[0])))

static bogostat_t bogostats;

static int compare_extrema(const void *id1, const void *id2)
{ 
    const discrim_t *d1 = id1;
    const discrim_t *d2 = id2;
    return ((d1->prob > d2->prob) ||
	     ((fabs(d1->prob - d2->prob) < EPS) && (strcmp(d1->key, d2->key) > 0)));
}

static void init_bogostats(/*@out@*/ bogostat_t *bogostats)
{
    size_t idx;

    for (idx = 0; idx < SIZEOF(bogostats->extrema); idx++)
    {
	discrim_t *pp = &bogostats->extrema[idx];
	pp->prob = EVEN_ODDS;
	pp->key[0] = '\0';
    }
}

static void populate_bogostats(/*@out@*/ bogostat_t *bogostats,
	const char *text, double prob,
	/*@unused@*/ int count)
/* if  the new word,prob pair is a better indicator.
 * add them to the bogostats structure */
{
    size_t idx;
    double dev;
    double slotdev, hitdev;
    discrim_t *pp, *hit;

    /* update the list of tokens with maximum deviation */
    dev = DEVIATION(prob);
    hit = NULL;
    hitdev=1;

    for (idx = 0; idx < SIZEOF(bogostats->extrema); idx++)
    {
	pp = &bogostats->extrema[idx];
	if (pp->key[0] == '\0' )
	{
	    hit = pp;
	    break;
	}
	else
	{
	    slotdev=DEVIATION(pp->prob);

	    if (dev>slotdev && hitdev>slotdev)
	    {
		hit=pp;
		hitdev=slotdev;
	    }
	}
    }
    if (hit) 
    { 
	if (verbose >= 3)
	{
	    int i = (hit - bogostats->extrema);
	    const char *curkey = hit->key[0] ? hit->key : "";
	    (void)fprintf(stderr, 
		    "#  %2d"
		    "  %f  %f  %-20.20s "
		    "  %f  %f  %s\n", i,
		   DEVIATION(prob),  prob, text,
		   DEVIATION(hit->prob), hit->prob, curkey);
	}
	hit->prob = prob;
	strncpy(hit->key, text, MAXTOKENLEN);
	hit->key[MAXTOKENLEN] = '\0';
    }
}

void gra_print_bogostats(FILE *fp, double spamicity)
{
    int idx = (thresh_index >= 0) ? thresh_index : KEEPERS+thresh_index;
    discrim_t *pp = &bogostats.extrema[idx];
    if (force || pp->prob >= thresh_stats)
	(void)gra_compute_spamicity( &bogostats, fp );
}

typedef struct {
    double good;
    double bad;
} wordprob_t;

static void wordprob_init(/*@out@*/ wordprob_t* wordstats)
{
    wordstats->good = wordstats->bad = 0.0;
}

static void wordprob_add(wordprob_t* wordstats, double newprob, int bad)
{
    if (bad)
	wordstats->bad+=newprob;
    else
	wordstats->good+=newprob;
}

static double wordprob_result(wordprob_t* wordstats)
{
    double prob = 0.0;
    double count = wordstats->good + wordstats->bad;

    prob = wordstats->bad/count;

    return (prob);
}

static double compute_probability(const char *token)
{
    wordlist_t* list;
    int override=0;
    long count;
    double prob;
    int totalcount=0;

    wordprob_t wordstats;

    wordprob_init(&wordstats);

    for (list=word_lists; list != NULL ; list=list->next)
    {
	if (override > list->override)
	    break;
	count=db_getvalue(list->dbh, token);

	if (count) {
	    if (list->ignore)
		return EVEN_ODDS;
	    totalcount+=count*list->weight;
	    override=list->override;
	    prob = (double)count;

	    prob /= list->msgcount;
	    prob *= list->weight;
	    prob = min(1.0, prob);
	    
	    wordprob_add(&wordstats, prob, list->bad);
	}
    }

    if (totalcount < MINIMUM_FREQ) {
	prob=UNKNOWN_WORD;
    } else {
	prob=wordprob_result(&wordstats);
	prob = min(MAX_PROB, prob);
	prob = max(MIN_PROB, prob);
    }

    return prob;
}

static bogostat_t *select_indicators(wordhash_t *wordhash)
/* selects the best spam/nonspam indicators and
 * populates the bogostats structure.
 */
{
    int w_count = 0;
    hashnode_t *node;

    init_bogostats(&bogostats);

    for(node = wordhash_first(wordhash); node != NULL; node = wordhash_next(wordhash))
    {
	char *token = node->key;
	double prob = compute_probability( token );

	w_count += 1;
	populate_bogostats( &bogostats, token, prob, 1 );
    }

    return (&bogostats);
}

double gra_compute_spamicity(bogostat_t *bogostats, FILE *fp) /*@globals errno@*/
/* computes the spamicity of the words in the bogostat structure
 * returns:  the spamicity */
{
    double product, invproduct;
    double spamicity = 0.0;

    discrim_t *pp;

    if (verbose)
    {
	/* put the bogostats in ascending order by probability and alphabet */
	qsort(bogostats->extrema, SIZEOF(bogostats->extrema), sizeof(discrim_t), compare_extrema);
    }

    /* Bayes' theorem. */
    /* For discussion, see <http://www.mathpages.com/home/kmath267.htm>. */
    product = invproduct = 1.0f;
    for (pp = bogostats->extrema; pp < bogostats->extrema+SIZEOF(bogostats->extrema); pp++)
    {
	if (fabs(pp->prob) < EPS)
	    continue;

	product *= pp->prob;
	invproduct *= (1 - pp->prob);
	spamicity = product / (product + invproduct);

	if (fp != NULL)
	{
	    switch (verbose)
	    {
		case 0:
		case 1:
		    break;
		case 2:
		    fprintf(fp, "%s%f  %s\n", stats_prefix, pp->prob, pp->key);
		    break;
		case 3:
		    fprintf(fp, "%s%f  %f  %s\n", stats_prefix, pp->prob, spamicity, pp->key);
		    break;
		default:
		    fprintf(fp, "%s%f  %f  %f  %8.5e  %s\n", stats_prefix, pp->prob, product, invproduct, spamicity, pp->key);
		    break;
	    }
	}
    }

    return spamicity;
}

void gra_initialize_constants(void)
{
    max_repeats = GRAHAM_MAX_REPEATS;
    if (fabs(min_dev) < EPS)
	min_dev = GRAHAM_MIN_DEV;
    if (spam_cutoff < EPS)
	spam_cutoff = GRAHAM_SPAM_CUTOFF;
    set_good_weight( GRAHAM_GOOD_BIAS );
}

double gra_bogofilter(wordhash_t *wordhash, FILE *fp) /*@globals errno@*/
{
    double spamicity;
    bogostat_t	*bogostats;

    /* select the best spam/nonspam indicators. */
    bogostats = select_indicators(wordhash);

    spamicity = gra_compute_spamicity(bogostats, fp);

    return spamicity;
}

void gra_cleanup(void)
{
}

/* Done */
