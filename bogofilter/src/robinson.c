/* $Id$ */

/*****************************************************************************

NAME:
   robinson.c -- implements f(w) and S, or Fisher, algorithm for computing spamicity.

******************************************************************************/

#include "common.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "bogoconfig.h"
#include "bogofilter.h"
#include "collect.h"
#include "datastore.h"
#include "degen.h"
#include "msgcounts.h"
#include "robinson.h"
#include "rstats.h"
#include "wordhash.h"
#include "wordlists.h"

extern int Rtable;

double	thresh_rtable = 0.0;		/* used in fisher.c */
double	robx = 0.0;			/* used in fisher.c and rstats.c */
double	robs = 0.0;			/* used in fisher.c and rstats.c */

#define ROBX_S ".ROBX"

static bool	need_stats = false;
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

double bad_cnt;
double good_cnt;

rf_method_t rf_robinson_method = {	/* needed by config.c */
    {
	"robinson",			/* const char		  *name;		*/
	rob_parm_table,	 		/* m_parm_table		  *parm_table		*/
	rob_initialize_constants,	/* m_initialize_constants *initialize_constants	*/
	rob_compute_spamicity, 		/* m_compute_spamicity	  *compute_spamicity	*/
	mth_spamicity,			/* m_spamicity		  *spamicity		*/
	mth_status,			/* m_status		  *status		*/
	rob_print_stats, 		/* m_print_bogostats	  *print_stats		*/
	rob_cleanup, 			/* m_free		  *cleanup		*/
    },
    rob_get_spamicity,			/* rf_get_spamicity	  *get_spamicity	*/
    rob_print_summary			/* rf_print_summary	  *print_summary	*/
};

/* Function Definitions */

void rob_print_stats(FILE *fp)
{
    fp = NULL; 	/* quench compiler warning */
    if (force || Rtable || verbose>=3 ||
	rob_stats.s.spamicity > thresh_stats || 
	rob_stats.s.spamicity > thresh_rtable || 
	(*method->status)() == RC_UNSURE )
	rstats_print();
}

static void wordprob_add(wordcnts_t *cnts, uint count, uint bad)
{
    if (bad)
	cnts->bad += count;
    else
	cnts->good += count;
}

double calc_prob(uint good, uint bad)
{
    uint g = min(good, msgs_good);
    uint b = min(bad,  msgs_bad);
    int n = g + b;
    double fw;

    if (n == 0)
	fw = robx;
    else {
	double pw = ((b / bad_cnt) / (b / bad_cnt + g / good_cnt));
	fw = (robs * robx + n * pw) / (robs + n);
    }

    return fw;
}

static double wordprob_result(wordcnts_t *cnt)
{
    double fw = calc_prob(cnt->good, cnt->bad);
    return fw;
}

static void lookup(const word_t *token, wordcnts_t *cnts)
{
    int override=0;
    wordlist_t* list;

    for (list=word_lists; list != NULL; list=list->next)
    {
	size_t i;
	dsv_t val;

	if (list->ignore)
	    return;

	if (override > list->override)
	    break;
	override=list->override;

	ds_read(list->dsh, token, &val);

	for (i=0; i<COUNTOF(val.count); i++) {
	    /* Protect against negatives */
	    if ((int) val.count[i] < 0) {
		val.count[i] = 0;
		ds_write(list->dsh, token, &val);
	    }

	    if (val.count[i] == 0)
		continue;

	    wordprob_add(cnts, val.count[i], list->bad[i]);
	    if (DEBUG_ROBINSON(1)) {
		fprintf(dbgout, "%2d %2d \n", (int) cnts->good, (int) cnts->bad);
		word_puts(token, 0, dbgout);
		fputc('\n', dbgout);
	    }
	}
    }
    return;
}

double lookup_and_score(const word_t *token, wordcnts_t *cnts)
{
    double prob;

    if (cnts->bad == 0 && cnts->good == 0)
	lookup(token, cnts);

    if (header_degen && token->leng >= 5 && memcmp(token->text, "head:", 5) == 0) {
	word_t *tword = word_new(token->text+5, token->leng-5);
	wordcnts_t tcnts;
	wordcnts_init(&tcnts);
	lookup_and_score(tword, &tcnts);
	wordcnts_incr(cnts, &tcnts);
	prob = wordprob_result(cnts);
	word_free(tword);
    }
    else
    if (cnts->bad != 0 || cnts->good != 0) {
	prob = wordprob_result(cnts);
    }
    else
    if (degen_enabled)
    {
	degen_enabled = false;	/* Disable further recursion */
	prob = degen(token, cnts);
	degen_enabled = true;	/* Enable further recursion */
    }
    else
	prob = wordprob_result(cnts);

    return prob;
}

static double compute_probability(const word_t *token, wordcnts_t *cnts)
{
    double prob;

    if (cnts->bad != 0 || cnts->good != 0 || msg_count_file)
	/* A msg-count file already has the values needed */
	prob = wordprob_result(cnts);
    else
	/* Otherwise lookup the word and get its score */
	prob = lookup_and_score(token, cnts);

    if (need_stats)
	rstats_add(token, prob, cnts);

    return prob;
}

double rob_compute_spamicity(wordhash_t *wh, FILE *fp) /*@globals errno@*/
/* selects the best spam/non-spam indicators and calculates Robinson's S */
{
    hashnode_t *node;

    FLOAT P = {1.0, 0};		/* Robinson's P */
    FLOAT Q = {1.0, 0};		/* Robinson's Q */

    double spamicity;
    size_t robn = 0;
    size_t count = 0;

    (void) fp; 	/* quench compiler warning */

    bad_cnt  = (double) max(1, msgs_bad);
    good_cnt = (double) max(1, msgs_good);

    if (DEBUG_ROBINSON(2)) fprintf(dbgout, "### rob_compute_spamicity() begins\n");

    need_stats = !no_stats && (Rtable || (verbose + passthrough) > 1);
    Rtable |= verbose > 3;

    if (need_stats)
	rstats_init();

    if (DEBUG_ROBINSON(2)) fprintf(dbgout, "min_dev: %f, robs: %f, robx: %f\n", 
				   min_dev, robs, robx);

    wordhash_sort(wh);

    for(node = wordhash_first(wh); node != NULL; node = wordhash_next(wh))
    {
	double prob;
	word_t *token;
	wordcnts_t *cnts;
	wordprop_t *props;

	if (!fBogotune) {
	    props = (wordprop_t *) node->buf;
	    cnts  = &props->cnts;
	    token = node->key;
	}
	else {
	    cnts = (wordcnts_t *) node;
	    token = NULL;
	}

	prob = compute_probability(token, cnts);

	count += 1;

	/* Robinson's P and Q; accumulation step */
        /*
	 * P = 1 - ((1-p1)*(1-p2)*...*(1-pn))^(1/n)     [spamminess]
         * Q = 1 - (p1*p2*...*pn)^(1/n)                 [non-spamminess]
	 */
        if (fabs(EVEN_ODDS - prob) - min_dev >= EPS) {
	    int e;

	    P.mant *= 1-prob;
	    if (P.mant < 1.0e-200) {
		P.mant = frexp(P.mant, &e);
		P.exp += e;
	    }

	    Q.mant *= prob;
	    if (Q.mant < 1.0e-200) {
		Q.mant = frexp(Q.mant, &e);
		Q.exp += e;
	    }
            robn ++;
        }

	if (DEBUG_ROBINSON(3)) {
	    (void)fprintf(dbgout, "%3lu %3lu %f ",
			  (unsigned long)robn, (unsigned long)count, prob);
	    (void)word_puts(token, 0, dbgout);
	    (void)fputc('\n', dbgout);
	}
    }

    /* Robinson's P, Q and S
    ** S = (P - Q) / (P + Q)                        [combined indicator]
    */

    spamicity = (*((rf_method_t *)method)->get_spamicity)( robn, P, Q );

    if (robn && need_stats)
	rstats_fini(robn, P, Q, spamicity );

    if (DEBUG_ROBINSON(2)) fprintf(dbgout, "### rob_compute_spamicity() ends\n");

    rob_stats.s.spamicity = spamicity;

    return (spamicity);
}

double rob_get_spamicity(size_t robn, FLOAT P, FLOAT Q)
{
    if (robn == 0)
	rob_stats.s.spamicity = robx;
    else
    {
	double r = 1.0 / robn;
	static double ln2 = 0.6931472;				/* ln(2) */

	rob_stats.robn = robn;

	/* convert to natural logs */
	rob_stats.p_ln = log(P.mant) + P.exp * ln2;		/* invlogsum */
	rob_stats.q_ln = log(Q.mant) + Q.exp * ln2;		/* logsum    */

	rob_stats.p_pr = 1.0 - exp(rob_stats.p_ln * r);		/* Robinson's P */
	rob_stats.q_pr = 1.0 - exp(rob_stats.q_ln * r);		/* Robinson's Q */

	rob_stats.s.spamicity = (1.0 + (rob_stats.p_pr - rob_stats.q_pr) / (rob_stats.p_pr + rob_stats.q_pr)) / 2.0;
    }

    return rob_stats.s.spamicity;
}

void rob_print_summary(void)
{
    if (!Rtable) {
	(void)fprintf(stdout, "%-*s %5lu %9.6f %9.6f %9.6f\n",
		      MAXTOKENLEN+2, "P_Q_S_invs_logs_md", 
		      (unsigned long)rob_stats.robn,
		      rob_stats.p_pr, rob_stats.q_pr, rob_stats.s.spamicity);
	(void)fprintf(stdout, "%-*s %9.2e %9.2e %6.3f\n",
		      MAXTOKENLEN+2+6, " ", robs, robx, min_dev);
    }
    else
	(void)fprintf(stdout, "%-*s %5lu %9.6f %9.6f %9.6f %9.3f %9.3f %5.3f\n",
		      MAXTOKENLEN+2, "P_Q_S_invs_logs_md", 
		      (unsigned long)rob_stats.robn,
		      rob_stats.p_pr, rob_stats.q_pr, rob_stats.s.spamicity, 
		      rob_stats.p_ln, rob_stats.q_ln, min_dev);
}

void rob_initialize_with_parameters(rob_stats_t *stats, double _min_dev, double _spam_cutoff)
{
    word_t *word_robx = word_new((const byte *)ROBX_W, strlen(ROBX_W));

    mth_initialize( stats, ROBINSON_MAX_REPEATS, _min_dev, _spam_cutoff, ROBINSON_GOOD_BIAS );

    /*
    ** If we're classifying messages, we need to compute the scalefactor 
    ** (from the .MSG_COUNT values)
    ** If we're registering tokens, we needn't get .MSG_COUNT
    */

    compute_msg_counts();
    if (fabs(robs) < EPS)
	robs = ROBS;

    if (fabs(robx) < EPS && word_list->dsh != NULL)
    {
	int ret;
	dsv_t val;

	/* Note: .ROBX is scaled by 1000000 in the wordlist */
	ret = ds_read(word_list->dsh, word_robx, &val);
	if (ret != 0)
	    robx = ROBX;
	else {
	    /* If found, unscale; else use predefined value */
	    uint l_robx = val.count[IX_SPAM];
	    robx = l_robx ? (double)l_robx / 1000000 : ROBX;
	}
    }

    if (robx < 0.0 || 1.0 < robx) {
	fprintf(stderr, "Invalid robx value (%f).  Must be between 0.0 and 1.0\n", robx);
	exit(EX_ERROR);
    }

    word_free(word_robx);

    return;
}

void rob_initialize_constants(void)
{
    rob_initialize_with_parameters(&rob_stats, ROBINSON_MIN_DEV, ROBINSON_SPAM_CUTOFF);
}

void rob_cleanup(void)
{
    /* Not yet implemented. */
}

/* Done */
