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
#include "bogofilter.h"
#include "datastore.h"
#include "robinson.h"
#include "rstats.h"
#include "wordhash.h"
#include "globals.h"

#include <assert.h>

#define EPS		(100.0 * DBL_EPSILON) /* equality cutoff */

#ifdef	ENABLE_GRAHAM_METHOD_XXX
/* constants for the Graham formula */
#define KEEPERS		15		/* how many extrema to keep */
#define MINIMUM_FREQ	5		/* minimum freq */

#define MAX_PROB	0.99f		/* max probability value used */
#define MIN_PROB	0.01f		/* min probability value used */
#define DEVIATION(n)	fabs((n) - EVEN_ODDS)	/* deviation from average */
#endif

#define ROBINSON_MIN_DEV	0.0f	/* if nonzero, use characteristic words */

#define ROBINSON_SPAM_CUTOFF	0.54f	/* if it's spammier than this... */

#define ROBINSON_MAX_REPEATS	1	/* cap on word frequency per message */
  
#define ROBS			0.001f	/* Robinson's s */
#define ROBX			0.415f	/* Robinson's x */

int max_repeats;
double spam_cutoff;

extern double min_dev;
extern double robs;
extern double robx;

extern int Rtable;
static double scalefactor;

void rob_print_bogostats(FILE *fp, double spamicity)
{
    if (force || spamicity > thresh_stats || spamicity > thresh_rtable)
	rstats_print();
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

#ifdef	ENABLE_GRAHAM_METHOD_XXX
    if (algorithm == AL_GRAHAM)
    {
	prob = wordstats->bad/count;
    }
#endif

#ifdef	ENABLE_ROBINSON_METHOD
    if (algorithm == AL_ROBINSON)
    {
	prob = ((ROBS * ROBX + wordstats->bad) / (ROBS + count));
    }
#endif

    return (prob);
}

static double compute_scale(void)
{
    wordlist_t* list;
    long goodmsgs=0L, badmsgs=0L;
    
    for(list=word_lists; list != NULL; list=list->next)
    {
	if (list->bad)
	    badmsgs += list->msgcount;
	else
	    goodmsgs += list->msgcount;
    }

    if (goodmsgs == 0L)
	return(1.0f);
    else
	return ((double)badmsgs / (double)goodmsgs);
}

static double compute_probability(const char *token)
{
    wordlist_t* list;
    int override=0;
    long count;
    double prob;
#ifdef	ENABLE_GRAHAM_METHOD_XXX
    int totalcount=0;
#endif

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
#ifdef	ENABLE_GRAHAM_METHOD_XXX
	    totalcount+=count*list->weight;
#endif
	    override=list->override;
	    prob = (double)count;

#ifdef	ENABLE_GRAHAM_METHOD_XXX
	    if (algorithm == AL_GRAHAM)
	    {
 	        prob /= list->msgcount;
 	        prob *= list->weight;
		prob = min(1.0, prob);
	    }
#endif

#ifdef	ENABLE_ROBINSON_METHOD
	    if (algorithm == AL_ROBINSON)
	    {
 	        if (!list->bad)
		    prob *= scalefactor;
	    }
#endif

	    wordprob_add(&wordstats, prob, list->bad);
	}
    }

#ifdef	ENABLE_GRAHAM_METHOD_XXX
    if (algorithm == AL_GRAHAM)
    {
	if (totalcount < MINIMUM_FREQ) {
	    prob=UNKNOWN_WORD;
	} else {
	    prob=wordprob_result(&wordstats);
	    prob = min(MAX_PROB, prob);
	    prob = max(MIN_PROB, prob);
	}
    }
#endif

#ifdef	ENABLE_ROBINSON_METHOD
    if (algorithm == AL_ROBINSON)
    {
	prob=wordprob_result(&wordstats);
	if ((Rtable || verbose) &&
	    (fabs(EVEN_ODDS - prob) >= min_dev))
	    rstats_add(token, wordstats.good, wordstats.bad, prob);
    }
#endif

    return prob;
}


#ifdef	ENABLE_ROBINSON_METHOD
double	rob_compute_spamicity(wordhash_t *wordhash, FILE *fp) /*@globals errno@*/
/* selects the best spam/nonspam indicators and calculates Robinson's S */
{
    hashnode_t *node;

    double invlogsum = 0.0;	/* Robinson's P */
    double logsum = 0.0;	/* Robinson's Q */
    double spamicity;
    int robn = 0;

    Rtable |= verbose > 3;

    if (fabs(robx) < EPS)
    {
	/* Note: .ROBX is scaled by 1000000 in the wordlist */
	long l_robx = db_getvalue(spam_list.dbh, ".ROBX");

	/* If found, unscale; else use predefined value */
	robx = l_robx ? (double)l_robx / 1000000 : ROBX;
    }

    if (Rtable || verbose)
	rstats_init();

    for(node = wordhash_first(wordhash); node != NULL; node = wordhash_next(wordhash))
    {
	char *token = node->key;
	double prob = compute_probability( token );

	/* Robinson's P and Q; accumulation step */
        /*
	 * P = 1 - ((1-p1)*(1-p2)*...*(1-pn))^(1/n)     [spamminess]
         * Q = 1 - (p1*p2*...*pn)^(1/n)                 [non-spamminess]
	 */
        if (fabs(EVEN_ODDS - prob) >= min_dev) {
            invlogsum += log(1.0 - prob);
	    logsum += log(prob);
            robn ++;
        }
    }

    /* Robinson's P, Q and S */
    /* S = (P - Q) / (P + Q)                        [combined indicator]
     */
    if (robn) {
	double invn = (double)robn;
	double invproduct = 1.0 - exp(invlogsum / invn);
	double product = 1.0 - exp(logsum / invn);

        spamicity =
            (1.0 + (invproduct - product) / (invproduct + product)) / 2.0;

	if (Rtable || verbose)
	    rstats_fini(robn, invlogsum, logsum, invproduct, product, spamicity);
    } else
	spamicity = robx;

    return (spamicity);
}
#endif

void rob_initialize_constants(void)
{
#ifdef	ENABLE_GRAHAM_METHOD_XXX
    if (algorithm == AL_GRAHAM)
    {
	spam_cutoff = GRAHAM_SPAM_CUTOFF;
	max_repeats = GRAHAM_MAX_REPEATS;
	if (fabs(min_dev) < EPS)
	    min_dev     = GRAHAM_MIN_DEV;
    }
#endif

#ifdef	ENABLE_ROBINSON_METHOD
    if (algorithm == AL_ROBINSON)
    {
	spam_cutoff = ROBINSON_SPAM_CUTOFF;
	max_repeats = ROBINSON_MAX_REPEATS;
	scalefactor = compute_scale();
	if (fabs(min_dev) < EPS)
	    min_dev     = ROBINSON_MIN_DEV;
	if (fabs(robs) < EPS)
	    robs = ROBS;
    }
#endif
}

double	rob_bogofilter(wordhash_t *wordhash, FILE *fp) /*@globals errno@*/
{
    double spamicity;
    spamicity = rob_compute_spamicity(wordhash, fp);
    return spamicity;
}

/* Done */
