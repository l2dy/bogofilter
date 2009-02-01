/* $Id$ */

/*****************************************************************************

NAME:
   rstats.c -- routines for printing robinson data for debugging.

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#include "common.h"

#include <math.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "bogofilter.h"
#include "msgcounts.h"
#include "prob.h"
#include "rstats.h"
#include "score.h"
#include "xmalloc.h"

typedef struct rstats_s rstats_t;
struct rstats_s {
    rstats_t *next;
    const word_t *token;
    u_int32_t	good;
    u_int32_t	bad;
    u_int32_t	msgs_good;
    u_int32_t	msgs_bad;
    bool   used;
    double prob;
};

typedef struct rhistogram_s rhistogram_t;
struct rhistogram_s {
    size_t count;
    double prob;
    double spamicity;
};

typedef struct header_s header_t;
struct header_s {
    rstats_t *list;
    uint      count;		/* words in list */
    uint      robn;		/* words in score */
    FLOAT     p;		/* Robinson's P */
    FLOAT     q;		/* Robinson's Q */
    double    min_dev;
    double    spamicity;
};

static header_t *stats_head = NULL;
static rstats_t *stats_tail = NULL;

/* Function Prototypes */

static void rstats_print_histogram(size_t robn, rstats_t **rstats_array, size_t count);
static void rstats_print_rtable(rstats_t **rstats_array, size_t count);

/* Function Definitions */

void rstats_init(void)
{
    if (stats_head == NULL) {
	stats_head = xcalloc(1, sizeof(header_t));
	stats_tail = (rstats_t *) xcalloc( 1, sizeof(rstats_t));
	stats_head->list = stats_tail;
    }
}

void rstats_cleanup(void)
{
    rstats_t *p, *q;

    for (p = stats_head->list; p != NULL; p = q)
    {
      q = p->next;
      xfree(p);
    }
    xfree(stats_head);
    stats_head = NULL;
    stats_tail = NULL;
}

void rstats_add(const word_t *token, double prob, bool used, wordcnts_t *cnts)
{
    if (token == NULL)
	return;

    stats_head->count += 1;
    stats_tail->next  = NULL;
    /* Using externally controlled data;
       token must not be freed before calling rstats_cleanup()
    */
    stats_tail->token = token;
    stats_tail->prob  = prob;
    stats_tail->used  = used;
    stats_tail->good  = cnts->good;
    stats_tail->bad   = cnts->bad;
    stats_tail->msgs_good = cnts->msgs_good;
    stats_tail->msgs_bad = cnts->msgs_bad;
    stats_tail->next = (rstats_t *)xcalloc(1, sizeof(rstats_t));
    stats_tail = stats_tail->next;
}

static int compare_rstats_t(const void *const ir1, const void *const ir2)
{
    const rstats_t *r1 = *(const rstats_t *const *)ir1;
    const rstats_t *r2 = *(const rstats_t *const *)ir2;

    if (r1->prob - r2->prob > EPS) return 1;
    if (r2->prob - r1->prob > EPS) return -1;

    return word_cmp(r1->token, r2->token);
}

#define	INTERVALS	10

void rstats_fini(size_t robn, FLOAT P, FLOAT Q, double spamicity)
{
    stats_head->robn      = robn;
    stats_head->p         = P;
    stats_head->q         = Q;
    stats_head->spamicity = spamicity;
}

void rstats_print(bool unsure)
{
    size_t r;
    size_t robn  = stats_head->robn;
    size_t count = stats_head->count;
    rstats_t *cur;
    rstats_t **rstats_array = (rstats_t **) xcalloc(count, sizeof(rstats_t *));

    for (r=0, cur=stats_head->list; r<count; r+=1, cur=cur->next)
	rstats_array[r] = cur;

    /* sort by ascending probability, then name */
    qsort(rstats_array, count, sizeof(rstats_t *), compare_rstats_t);

    if (Rtable || verbose>=3)
	rstats_print_rtable(rstats_array, count);
    else if (verbose==2 || (unsure && verbose))
	rstats_print_histogram(robn, rstats_array, count);

    xfree(rstats_array);
}

static void rstats_print_histogram(size_t robn, rstats_t **rstats_array, size_t count)
{
    size_t i, r;
    size_t maxcnt=0;
    rhistogram_t hist[INTERVALS];

    double invn = (double) robn;

    double invlogsum = 0.0;	/* Robinson's P */
    double logsum = 0.0;	/* Robinson's Q */

    if (!stats_in_header)
	(void)fprintf(fpo, "\n" );

    /* Compute histogram */
    for (i=r=0; i<INTERVALS; i+=1)
    {
	rhistogram_t *h = &hist[i];
	double fin = 1.0*(i+1)/INTERVALS;
	size_t cnt = 0;
	h->prob = 0.0;
	h->spamicity=0.0;
	while (r < count)
	{
	    rstats_t *cur = rstats_array[r];
	    double prob = cur->prob;
	    if (prob >= fin)
		break;

	    if (cur->used)
	    {
		cnt += 1;
		h->prob += prob;
		invlogsum += log(1.0 - prob);
		logsum += log(prob);
	    }

	    r += 1;
	}

	if (robn == 0)
	    h->spamicity = robx;
	else 
	{
	    double invproduct, product;
	    invproduct = 1.0 - exp(invlogsum / invn);
	    product = 1.0 - exp(logsum / invn);
	    h->spamicity = (invproduct + product < EPS) 
		? 0.0 
		: (1.0 + (invproduct - product) / (invproduct + product)) / 2.0;
	}
	h->count=cnt;
	maxcnt = max(maxcnt, cnt);
    }

    (void)fprintf(fpo, "%s%4s %4s %6s  %9s %s\n", stats_prefix, "int", "cnt", "prob", "spamicity", "histogram" );

    /* Print histogram */
    for (i=0; i<INTERVALS; i+=1)
    {
	double beg = 1.0*i/INTERVALS;
	rhistogram_t *h = &hist[i];
	size_t cnt = h->count;
	double prob = cnt ? h->prob/cnt : 0.0;

	/* print interval, count, probability, and spamicity */
	(void)fprintf(fpo, "%s%3.2f %4lu %f %f ", stats_prefix, beg, (unsigned long)cnt, prob, h->spamicity );

	/* scale histogram to 48 characters */
	if (maxcnt>48) cnt = (cnt * 48 + maxcnt - 1) / maxcnt;

	/* display histogram */
	for (r=0; r<cnt; r+=1)
	    (void)fputc( '#', fpo);
	(void)fputc( '\n', fpo);
    }
}

static void rstats_print_rtable(rstats_t **rstats_array, size_t count)
{
    size_t r;
    const char *pfx = !stats_in_header ? "" : "  ";

    /* print header */
    if (!Rtable)
	(void)fprintf(fpo, "%s%*s %6s    %-6s    %-6s    %-6s %s\n",
		      pfx, max_token_len+2, "", "n", "pgood", "pbad", "fw", "U");
    else
	(void)fprintf(fpo, "%s%*s %6s    %-6s    %-6s    %-6s  %-6s    %-6s %s\n",
		      pfx, max_token_len+2, "", "n", "pgood", "pbad", "fw", "invfwlog", "fwlog", "U");

    /* Print 1 line per token */
    for (r= 0; r<count; r+=1)
    {
	rstats_t *cur = rstats_array[r];
	int len = (cur->token->leng >= max_token_len) ? 0 : (max_token_len - cur->token->leng);
	double fw = calc_prob(cur->good, cur->bad, cur->msgs_good, cur->msgs_bad);
	char flag = cur->used ? '+' : '-';

	(void)fprintf(fpo, "%s\"", pfx);
	(void)word_puts(cur->token, 0, fpo);

	if (cur->msgs_good == 0 && cur->msgs_bad == 0)
	{
	    flag = 'i';
	    (void)fprintf(fpo, "\"%*s %6lu  %8s  %8s  %8.6f",
			  len, " ", (unsigned long)(cur->good + cur->bad),
			  "--------", "--------",
			  fw);
	}
	else
	    (void)fprintf(fpo, "\"%*s %6lu  %8.6f  %8.6f  %8.6f",
			  len, " ", (unsigned long)(cur->good + cur->bad),
			  (double)cur->good / cur->msgs_good,
			  (double)cur->bad  / cur->msgs_bad,
			  fw);

	if (Rtable)
	    (void)fprintf(fpo, "%s%10.5f%10.5f",
			  pfx, log(1.0 - fw), log(fw));
	(void)fprintf(fpo, " %c\n", flag);
    }

    /* print trailer */
    msg_print_summary(pfx);
}
