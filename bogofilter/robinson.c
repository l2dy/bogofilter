/* $Id$ */

/*****************************************************************************

NAME:
   graham.c -- code for implementing Graham algorithm for computing spamicity.

******************************************************************************/

#include <math.h>
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

#define ROBINSON_MIN_DEV	0.0f	/* if nonzero, use characteristic words */
#define ROBINSON_SPAM_CUTOFF	0.54f	/* if it's spammier than this... */

#define ROBINSON_MAX_REPEATS	1	/* cap on word frequency per message */
  
#define ROBS			0.001f	/* Robinson's s */
#define ROBX			0.415f	/* Robinson's x */

extern double min_dev;
extern double robs;
extern double robx;

extern int Rtable;
static double scalefactor;

method_t robinson_method = {
    rob_initialize_constants, 		/* alg_initialize_constants *initialize_constants; */
    rob_bogofilter,	 		/* alg_compute_spamicity *compute_spamicity; */
    rob_print_bogostats, 		/* alg_print_bogostats *print_stats; */
    rob_cleanup 			/* alg_free *cleanup; */
} ;

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

    prob = ((ROBS * ROBX + wordstats->bad) / (ROBS + count));

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
	    override=list->override;
	    prob = (double)count;

	    if (!list->bad)
		prob *= scalefactor;

	    wordprob_add(&wordstats, prob, list->bad);
	}
    }

    prob=wordprob_result(&wordstats);
    if ((Rtable || verbose) &&
	(fabs(EVEN_ODDS - prob) >= min_dev))
	rstats_add(token, wordstats.good, wordstats.bad, prob);

    return prob;
}

double rob_compute_spamicity(wordhash_t *wordhash, FILE *fp) /*@globals errno@*/
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

void rob_initialize_constants(void)
{
    max_repeats = ROBINSON_MAX_REPEATS;
    scalefactor = compute_scale();
    if (fabs(min_dev) < EPS)
	min_dev = ROBINSON_MIN_DEV;
    if (fabs(robs) < EPS)
	robs = ROBS;
    if (spam_cutoff < EPS)
	spam_cutoff = ROBINSON_SPAM_CUTOFF;
    set_good_weight( ROBINSON_GOOD_BIAS );
}

double rob_bogofilter(wordhash_t *wordhash, FILE *fp) /*@globals errno@*/
{
    double spamicity;
    spamicity = rob_compute_spamicity(wordhash, fp);
    return spamicity;
}

void rob_cleanup(void)
{
}

/* Done */
