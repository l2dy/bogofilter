/* $Id$ */

/*****************************************************************************

NAME:
   graham.c -- code for implementing graham algorithm for computing spamicity.

******************************************************************************/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <config.h>
#include "common.h"

#include "bogofilter.h"
#include "datastore.h"
#include "graham.h"
#include "lexer.h"
#include "method.h"
#include "wordhash.h"

/* constants for the Graham formula */
#define KEEPERS		15		/* how many extrema to keep */
#define MINIMUM_FREQ	5		/* minimum freq */

#define MAX_PROB	0.99f		/* max probability value used */
#define MIN_PROB	0.01f		/* min probability value used */

#define GRAHAM_MIN_DEV		0.4f	/* look for characteristic words */
#define GRAHAM_SPAM_CUTOFF	0.90f	/* if it's spammier than this... */
#define GRAHAM_MAX_REPEATS	4	/* cap on word frequency per message */

extern double min_dev;

int	thresh_index = 0;

stats_t stats;

static const parm_desc gra_parm_table[] =
{
    { "thresh_index",	  CP_INTEGER,	{ (void *) &thresh_index } },
    { NULL,		  CP_NONE,	{ (void *) NULL } },
};

static	rc_t	gra_status(void);

method_t graham_method = {
    "graham",				/* const char		  *name;		*/
    gra_parm_table,	 		/* m_parm_table		  *parm_table		*/
    gra_initialize_constants, 		/* m_initialize_constants *initialize_constants	*/
    gra_bogofilter,	 		/* m_compute_spamicity	  *compute_spamicity	*/
    gra_status, 			/* m_status		  *status		*/
    gra_print_bogostats, 		/* m_print_bogostats	  *print_stats		*/
    gra_cleanup 			/* m_free		  *cleanup		*/
} ;

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

    if (d1->prob > d2->prob) return 1;
    if (d1->prob < d2->prob) return -1;

    return strcmp(d1->key, d2->key);
}

static void init_bogostats(/*@out@*/ bogostat_t *bs)
{
    size_t idx;

    for (idx = 0; idx < SIZEOF(bs->extrema); idx++)
    {
	discrim_t *pp = &bs->extrema[idx];
	pp->prob = EVEN_ODDS;
	pp->key[0] = '\0';
    }
}

static void populate_bogostats(/*@out@*/ bogostat_t *bs,
	const char *text, double prob)
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
    hitdev=slotdev=1;

    for (idx = 0; idx < SIZEOF(bs->extrema); idx++)
    {
	pp = &bs->extrema[idx];
	if (pp->key[0] == '\0' )
	{
	    hit = pp;
	    break;
	}
	else
	{
	    slotdev=DEVIATION(pp->prob);
	    if (dev-slotdev>EPS && hitdev-slotdev>EPS)
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
	    int i = (hit - bs->extrema);
	    const char *curkey = hit->key[0] ? hit->key : "";
	    (void)fprintf(stderr, 
		    "#  %2d"
		    "  %f  %f  %-20.20s "
		    "  %f  %f  %s\n", i,
		   DEVIATION(prob),  prob, text,
		   DEVIATION(hit->prob), hit->prob, curkey);
	}
	hit->prob = prob;
	if (strlcpy(hit->key, text, MAXTOKENLEN) >= MAXTOKENLEN) {
	    /* The lexer should not have returned a token longer than
	     * MAXTOKENLEN */
	    internal_error;
	    abort();
	}
    }
    return;
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
	populate_bogostats( &bogostats, token, prob );
    }

    return (&bogostats);
}

double gra_compute_spamicity(bogostat_t *bs, FILE *fp) /*@globals errno@*/
/* computes the spamicity of the words in the bogostat structure
 * returns:  the spamicity */
{
    double product, invproduct;
    double spamicity = 0.0;

    discrim_t *pp;

    if (verbose)
    {
	/* put the bs in ascending order by probability and alphabet */
	qsort(bs->extrema, SIZEOF(bs->extrema), sizeof(discrim_t), compare_extrema);
    }

    /* Bayes' theorem. */
    /* For discussion, see <http://www.mathpages.com/home/kmath267.htm>. */
    product = invproduct = 1.0f;
    for (pp = bs->extrema; pp < bs->extrema+SIZEOF(bs->extrema); pp++)
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

    stats.spamicity = spamicity;

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

rc_t gra_status(void)
{
    rc_t status = ( stats.spamicity >= spam_cutoff ) ? RC_SPAM : RC_HAM;
    return status;
}

double gra_bogofilter(wordhash_t *wordhash, FILE *fp) /*@globals errno@*/
{
    double spamicity;
    bogostat_t	*bs;

    /* select the best spam/nonspam indicators. */
    bs = select_indicators(wordhash);
    spamicity = gra_compute_spamicity(bs, fp);

    return spamicity;
}

void gra_cleanup(void)
{
}

/* Done */
