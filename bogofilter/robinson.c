/* $Id$ */

/*****************************************************************************

NAME:
   robinson.c -- implements f(w) and S, or Fisher, algorithm for computing spamicity.

******************************************************************************/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <config.h>
#include "common.h"

#include "bogoconfig.h"
#include "bogofilter.h"
#include "datastore.h"
#include "robinson.h"
#include "rstats.h"
#include "wordhash.h"

#define ROBINSON_MIN_DEV	0.0f	/* if nonzero, use characteristic words */
#define ROBINSON_SPAM_CUTOFF	0.54f	/* if it's spammier than this... */

/* 11/17 - Greg Louis recommends the following values:
** #define ROBINSON_SPAM_CUTOFF	0.582f
** #define ROBINSON_MIN_DEV	0.1f
*/

#define ROBINSON_MAX_REPEATS	1	/* cap on word frequency per message */
  
#define ROBS			0.001f	/* Robinson's s */
#define ROBX			0.415f	/* Robinson's x */

extern int Rtable;
static double scalefactor;

double	thresh_rtable = 0.0f;		/* used in fis_parms in fisher.c */
double	robx = 0.0f;			/* used in fis_parms in fisher.c */
double	robs = 0.0f;			/* used in fis_parms in fisher.c */

static rob_stats_t  rob_stats;

const parm_desc rob_parm_table[] =	/* needed by fisher.c */
{
    { "robx",		  CP_DOUBLE,	{ (void *) &robx } },
    { "robs",		  CP_DOUBLE,	{ (void *) &robs } },
    { "thresh_rtable",	  CP_DOUBLE,	{ (void *) &thresh_rtable } },
    { NULL,		  CP_NONE,	{ (void *) NULL } },
};

/* Function Prototypes */

static void	rob_initialize_constants(void);
static double	rob_get_spamicity(size_t robn, FLOAT P, FLOAT Q);
static void	rob_print_summary(void);

/* Static Variables */

#ifdef	ENABLE_ROBINSON_METHOD
rf_method_t rf_robinson_method = {	/* needed by config.c */
    {
	"robinson",			/* const char		  *name;		*/
	rob_parm_table,	 		/* m_parm_table		  *parm_table		*/
	rob_initialize_constants,	/* m_initialize_constants *initialize_constants	*/
	rob_bogofilter,	 		/* m_compute_spamicity	  *compute_spamicity	*/
	mth_spamicity,			/* m_spamicity		  *spamicity		*/
	mth_status,			/* m_status		  *status		*/
	rob_print_stats, 		/* m_print_bogostats	  *print_stats		*/
	rob_cleanup, 			/* m_free		  *cleanup		*/
    },
    rob_get_spamicity,			/* rf_get_spamicity	  *get_spamicity	*/
    rob_print_summary			/* rf_print_summary	  *print_summary	*/
};
#endif

void rob_print_stats(FILE *fp)
{
    if (force || 
	rob_stats.s.spamicity > thresh_stats || 
	rob_stats.s.spamicity > thresh_rtable || 
	method->status() == RC_UNSURE )
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
    if (Rtable || verbose)
	rstats_add(token, wordstats.good, wordstats.bad, prob);

    return prob;
}


double rob_compute_spamicity(wordhash_t *wordhash, FILE *fp) /*@globals errno@*/
/* selects the best spam/nonspam indicators and calculates Robinson's S */
{
    hashnode_t *node;

    FLOAT P = {1.0, 0};		/* Robinson's P */
    FLOAT Q = {1.0, 0};		/* Robinson's Q */

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
	    P.mant *= 1-prob;
	    if (P.mant < 1.0e-200) {
		P.mant *= 1.0e200;
		P.exp -= 200;
	    }

	    Q.mant *= prob;
	    if (Q.mant < 1.0e-200) {
		Q.mant *= 1.0e200;
		Q.exp -= 200;
	    }
            robn ++;
        }
    }

    /* Robinson's P, Q and S
    ** S = (P - Q) / (P + Q)                        [combined indicator]
    */

    spamicity = ((rf_method_t *) method)->get_spamicity( robn, P, Q );

    if (robn && (Rtable || verbose))
	rstats_fini(robn, P, Q, spamicity );

    return (spamicity);
}

double rob_get_spamicity(size_t robn, FLOAT P, FLOAT Q)
{
    double r = 1.0 / (double)robn;
    double ln10 = 2.302585093;			/* log(10) - 2.3025850929940459  */

    rob_stats.robn = robn;

    rob_stats.p_ln = log(P.mant) + P.exp * ln10;	/* invlogsum */
    rob_stats.q_ln = log(Q.mant) + Q.exp * ln10;	/* logsum    */

    rob_stats.p_pr = 1.0 - pow(P.mant, r) * pow(10.0, P.exp * r);	/* Robinson's P */
    rob_stats.q_pr = 1.0 - pow(Q.mant, r) * pow(10.0, Q.exp * r);	/* Robinson's Q */

    rob_stats.s.spamicity = (1.0 + (rob_stats.p_pr - rob_stats.q_pr) / (rob_stats.p_pr + rob_stats.q_pr)) / 2.0;

    return rob_stats.s.spamicity;
}

void rob_print_summary(void)
{
    (void)fprintf(stdout, "%3d  %-20s %9.6f %9.6f %9.6f %9.3f %9.3f %4.2f\n",
		  rob_stats.robn+1, "P_Q_S_invs_logs_md", 
		  rob_stats.p_pr, rob_stats.q_pr, rob_stats.s.spamicity, rob_stats.p_ln, rob_stats.q_ln, min_dev);
}

void rob_initialize_with_parameters(rob_stats_t *stats, double _min_dev, double _spam_cutoff)
{
    mth_initialize( stats, ROBINSON_MAX_REPEATS, _min_dev, _spam_cutoff, ROBINSON_GOOD_BIAS );
    scalefactor = compute_scale();
    if (fabs(robs) < EPS)
	robs = ROBS;
}

void rob_initialize_constants(void)
{
    rob_initialize_with_parameters(&rob_stats, ROBINSON_MIN_DEV, ROBINSON_SPAM_CUTOFF);
}

double rob_bogofilter(wordhash_t *wordhash, FILE *fp) /*@globals errno@*/
{
    rob_stats.s.spamicity = rob_compute_spamicity(wordhash, fp);
    return rob_stats.s.spamicity;
}

void rob_cleanup(void)
{
    /* Not yet implemented. */
}

/* Done */
