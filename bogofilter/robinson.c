/* $Id$ */

/*****************************************************************************

NAME:
   robinson.c -- implements f(w) and S, or Fisher, algorithm for computing spamicity.

******************************************************************************/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "config.h"
#include "common.h"
#include "bogoconfig.h"
#include "bogofilter.h"
#include "datastore.h"
#include "robinson.h"
#include "rstats.h"
#include "wordhash.h"
#include "globals.h"

#define ROBINSON_MIN_DEV	0.0f	/* if nonzero, use characteristic words */
#define ROBINSON_SPAM_CUTOFF	0.54f	/* if it's spammier than this... */

/* 11/17/02:
**	   Greg Louis recommends:
** #define ROBINSON_MIN_DEV	0.1f
** #define ROBINSON_SPAM_CUTOFF	0.582f
*/

#define ROBINSON_MAX_REPEATS	1	/* cap on word frequency per message */
  
#define ROBS			0.001f	/* Robinson's s */
#define ROBX			0.415f	/* Robinson's x */

extern double min_dev;

extern int Rtable;
static double scalefactor;

static double	thresh_rtable = 0.0f;
static double	robx = 0.0f;
static double	robs = 0.0f;

const parm_desc rob_parm_table[] =	/* needed by fisher.c */
{
    { "robx",		  CP_DOUBLE,	{ (void *) &robx } },
    { "robs",		  CP_DOUBLE,	{ (void *) &robs } },
    { "thresh_rtable",	  CP_DOUBLE,	{ (void *) &thresh_rtable } },
    { NULL,		  CP_NONE,	{ (void *) NULL } },
};

#ifdef	ENABLE_ROBINSON_METHOD
rf_method_t rf_robinson_method = {
    {
	"robinson",			/* const char		  *name;		*/
	rob_parm_table,	 		/* m_parm_table		  *parm_table		*/
	rob_initialize_constants,	/* m_initialize_constants *initialize_constants	*/
	rob_bogofilter,	 		/* m_compute_spamicity	  *compute_spamicity	*/
	rob_print_bogostats, 		/* m_print_bogostats	  *print_stats		*/
	rob_cleanup, 			/* m_free		  *cleanup		*/
    },
    rob_get_spamicity			/* rf_get_spamicity	  *get_spamicity	*/
};
#endif

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
    size_t robn = 0;

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
	double invproduct, product;

	spamicity = ((rf_method_t *) method)->get_spamicity( robn, invlogsum, logsum, &invproduct, &product );

	if (Rtable || verbose)
	    rstats_fini(robn, invlogsum, logsum, invproduct, product, spamicity);
    } else
	spamicity = robx;

    return (spamicity);
}

double rob_get_spamicity(size_t robn, double invlogsum, double logsum, double *invproduct, double *product)
{
    double invn = (double)robn;
    double _invproduct = 1.0 - exp(invlogsum / invn);
    double _product = 1.0 - exp(logsum / invn);

    double spamicity = (1.0 + (_invproduct - _product) / (_invproduct + _product)) / 2.0;

    *product = _product;
    *invproduct = _invproduct;

    return spamicity;
}

void rob_initialize_with_parameters(double _min_dev, double _spam_cutoff)
{
    max_repeats = ROBINSON_MAX_REPEATS;
    scalefactor = compute_scale();
    if (fabs(min_dev) < EPS)
	min_dev = _min_dev;
    if (fabs(robs) < EPS)
	robs = ROBS;
    if (spam_cutoff < EPS)
	spam_cutoff = _spam_cutoff;
    set_good_weight( ROBINSON_GOOD_BIAS );
}

void rob_initialize_constants(void)
{
    rob_initialize_with_parameters(ROBINSON_MIN_DEV, ROBINSON_SPAM_CUTOFF);
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
