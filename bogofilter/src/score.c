/* $Id$ */

/*****************************************************************************

NAME:
   score.c -- implements Fisher variant on Robinson algorithm.

******************************************************************************/

#include "common.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "bogoconfig.h"
#include "bogofilter.h"
#include "collect.h"
#include "datastore.h"
#include "msgcounts.h"
#include "prob.h"
#include "rstats.h"
#include "score.h"
#include "wordhash.h"
#include "wordlists.h"

#if defined(HAVE_GSL_10) && !defined(HAVE_GSL_14)
/* HAVE_GSL_14 implies HAVE_GSL_10
 * if we have neither, we'll use our included GSL 1.4, which knows CDFs
 * if we have both, we have GSL 1.4, which knows CDFs
 *
 * in other cases, we need to integrate the PDF to get the CDF
 */
#define GSL_INTEGRATE_PDF
#include "gsl/gsl_randist.h"
#include "gsl/gsl_integration.h"
#include "gsl/gsl_errno.h"
#else
#include "gsl/gsl_cdf.h"
#endif

/* Function Prototypes */

static	double	get_spamicity(size_t robn, FLOAT P, FLOAT Q);

/* Static Variables */

static score_t  score;

/* Function Definitions */

double msg_spamicity(void)
{
    return score.spamicity;
}

rc_t msg_status(void)
{
    if (score.spamicity >= spam_cutoff)
	return RC_SPAM;

    if ((ham_cutoff < EPS) ||
	(score.spamicity <= ham_cutoff))
	return RC_HAM;

    return RC_UNSURE;
}

void msg_print_stats(FILE *fp)
{
    bool unsure = unsure_stats && (msg_status() == RC_UNSURE) && verbose;

    (void)fp;

    if (Rtable || unsure || verbose >= 2)
	rstats_print(unsure);
}

static void wordprob_add(wordcnts_t *cnts, uint count, uint bad)
{
    if (bad)
	cnts->bad += count;
    else
	cnts->good += count;
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

    if (fBogotune) {
	wordprop_t *wp = wordhash_search_memory(token);
	if (wp) {
	    cnts->good = wp->cnts.good;
	    cnts->bad  = wp->cnts.bad;
	}
	return;
    }

    for (list=word_lists; list != NULL; list=list->next)
    {
	size_t i;
	dsv_t val;

	if (override > list->override)	/* if already found */
	    break;

	if (ds_read(list->dsh, token, &val) != 0)
	    continue;			/* not found */

	if (list->ignore)		/* if on ignore list */
	    return;

	override=list->override;

	for (i=0; i<COUNTOF(val.count); i++) {
	    /* Protect against negatives */
	    if ((int) val.count[i] < 0) {
		val.count[i] = 0;
		ds_write(list->dsh, token, &val);
	    }
	    wordprob_add(cnts, val.count[i], list->bad[i]);
	}

	if (DEBUG_ALGORITHM(1)) {
	    fprintf(dbgout, "%2d %2d \n", (int) cnts->good, (int) cnts->bad);
	    word_puts(token, 0, dbgout);
	    fputc('\n', dbgout);
	}
    }

    return;
}

double msg_lookup_and_score(const word_t *token, wordcnts_t *cnts)
{
    double prob;

    if (cnts->bad == 0 && cnts->good == 0)
	lookup(token, cnts);

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
	prob = msg_lookup_and_score(token, cnts);

    return prob;
}

double msg_compute_spamicity(wordhash_t *wh, FILE *fp) /*@globals errno@*/
/* selects the best spam/non-spam indicators and calculates Robinson's S */
{
    hashnode_t *node;

    FLOAT P = {1.0, 0};		/* Robinson's P */
    FLOAT Q = {1.0, 0};		/* Robinson's Q */

    double spamicity;
    size_t robn = 0;
    size_t count = 0;
    bool need_stats;

    (void) fp; 	/* quench compiler warning */

    if (DEBUG_ALGORITHM(2)) fprintf(dbgout, "### msg_compute_spamicity() begins\n");

    Rtable |= verbose > 3;
    need_stats = Rtable || verbose || passthrough;

    if (need_stats)
	rstats_init();

    if (DEBUG_ALGORITHM(2)) fprintf(dbgout, "min_dev: %f, robs: %f, robx: %f\n", 
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

	count += 1;

	prob = compute_probability(token, cnts);

	if (need_stats)
	    rstats_add(token, prob, cnts);

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

	if (DEBUG_ALGORITHM(3)) {
	    (void)fprintf(dbgout, "%3lu %3lu %f ",
			  (unsigned long)robn, (unsigned long)count, prob);
	    (void)word_puts(token, 0, dbgout);
	    (void)fputc('\n', dbgout);
	}
    }

    /* Robinson's P, Q and S
    ** S = (P - Q) / (P + Q)                        [combined indicator]
    */

    spamicity = get_spamicity(robn, P, Q);

    if (need_stats && robn != 0)
	rstats_fini(robn, P, Q, spamicity );

    if (DEBUG_ALGORITHM(2)) fprintf(dbgout, "### msg_compute_spamicity() ends\n");

    return (spamicity);
}

void score_initialize(void)
{
    word_t *word_robx = word_new((const byte *)ROBX_W, strlen(ROBX_W));

    if (fabs(min_dev) < EPS)
	min_dev = MIN_DEV;
    if (spam_cutoff < EPS)
	spam_cutoff = SPAM_CUTOFF;

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

void score_cleanup(void)
{
    rstats_cleanup();
}

#ifdef GSL_INTEGRATE_PDF
static double chisq(double x, void *p) {
     return(gsl_ran_chisq_pdf(x, *(double *)p));
}

inline static double prbf(double x, double df) {
    gsl_function chi;
    int status;
    double p, abserr;
    const int intervals = 15;
    const double eps = 1000 * DBL_EPSILON;

    gsl_integration_workspace *w;
    chi.function = chisq;
    chi.params = &df;
    gsl_set_error_handler_off();
    w = gsl_integration_workspace_alloc(intervals);
    if (!w) {
	fprintf(stderr, "Out of memory! %s:%d\n", __FILE__, __LINE__);
	abort();
    }
    status = gsl_integration_qag(&chi, 0, x, eps, eps,
	    intervals, GSL_INTEG_GAUSS15, w, &p, &abserr);
    if (status && status != GSL_EMAXITER) {
	fprintf(stderr, "Integration error: %s\n", gsl_strerror(status));
	abort();
    }
    gsl_integration_workspace_free(w);
    p = max(0.0, 1.0 - p);
    return(min(1.0, p));
}
#else
inline static double prbf(double x, double df)
{
    double r = gsl_cdf_chisq_Q(x, df);
    return (r < DBL_EPSILON) ? 0.0 : r;
}
#endif

double get_spamicity(size_t robn, FLOAT P, FLOAT Q)
{
    if (robn == 0)
    {
	score.spamicity = robx;
    }
    else
    {
	double df = 2.0 * robn;
	double ln2 = log(2.0);					/* ln(2) */

	score.robn = robn;

	/* convert to natural logs */
	score.p_ln = log(P.mant) + P.exp * ln2;		/* invlogsum */
	score.q_ln = log(Q.mant) + Q.exp * ln2;		/* logsum    */

	score.p_pr = prbf(-2.0 * score.p_ln, df);	/* compute P */
	score.q_pr = prbf(-2.0 * score.q_ln, df);	/* compute Q */

	score.spamicity = (1.0 + score.q_pr - score.p_pr) / 2.0;
    }

    return score.spamicity;
}

void msg_print_summary(void)
{
    if (!Rtable) {
	(void)fprintf(fpo, "%-*s %5lu %9.2e %9.2e %9.2e\n",
		      MAXTOKENLEN+2, "N_P_Q_S_s_x_md", (unsigned long)score.robn, 
		      score.p_pr, score.q_pr, score.spamicity);
	(void)fprintf(fpo, "%-*s %9.2e %9.2e %6.3f\n",
		      MAXTOKENLEN+2+6, " ", robs, robx, min_dev);
    }
    else
	(void)fprintf(fpo, "%-*s %5lu %9.2e %9.2e %9.2e %9.2e %9.2e %5.3f\n",
		      MAXTOKENLEN+2, "N_P_Q_S_s_x_md", (unsigned long)score.robn, 
		      score.p_pr, score.q_pr, score.spamicity, robs, robx, min_dev);
}

/* Done */
