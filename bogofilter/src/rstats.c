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
    word_t *token;
    u_int32_t	good;
    u_int32_t	bad;
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
    double    spamicity;
};

static header_t  header;
static rstats_t *current = NULL;

/* Function Prototypes */

static void rstats_print_histogram(size_t robn, rstats_t **rstats_array, size_t count);
static void rstats_print_rtable(rstats_t **rstats_array, size_t count);

/* Function Definitions */

void rstats_init(void)
{
    if (current == NULL)
	current = (rstats_t *) xcalloc( 1, sizeof(rstats_t));
    header.list = current;
    header.count = 0;
    header.robn  = 0;
}

void rstats_cleanup(void)
{
    rstats_t *p, *q;

    for (p = header.list; p != NULL; p = q)
    {
      q = p->next;
      xfree(p->token);
      xfree(p);
    }
    current = NULL;
    header.list = NULL;
}

void rstats_add(const word_t *token, double prob, wordcnts_t *cnts)
{
    if (token == NULL)
	return;

    header.count += 1;
    current->next  = NULL;
    current->token = word_dup(token);
    current->prob  = prob;
    current->good  = cnts->good;
    current->bad   = cnts->bad;
    current->next = (rstats_t *)xcalloc(1, sizeof(rstats_t));
    current = current->next;
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
    header.robn       = robn;
    header.p          = P;
    header.q          = Q;
    header.spamicity  = spamicity;
}

void rstats_print(bool unsure)
{
    size_t r;
    size_t robn  = header.robn;
    size_t count = header.count;
    rstats_t *cur;
    rstats_t **rstats_array = (rstats_t **) xcalloc(count, sizeof(rstats_t *));

    for (r=0, cur=header.list; r<count; r+=1, cur=cur->next)
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
	(void)fprintf(stdout, "\n" );

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
	    double prob = rstats_array[r]->prob;
	    if (prob >= fin)
		break;

	    if (fabs(EVEN_ODDS - prob) - min_dev >= EPS)
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

    (void)fprintf(stdout, "%s%4s %4s %6s  %9s %s\n", stats_prefix, "int", "cnt", "prob", "spamicity", "histogram" );

    /* Print histogram */
    for (i=0; i<INTERVALS; i+=1)
    {
	double beg = 1.0*i/INTERVALS;
	rhistogram_t *h = &hist[i];
	size_t cnt = h->count;
	double prob = cnt ? h->prob/cnt : 0.0;

	/* print interval, count, probability, and spamicity */
	(void)fprintf(stdout, "%s%3.2f %4lu %f %f ", stats_prefix, beg, (unsigned long)cnt, prob, h->spamicity );

	/* scale histogram to 48 characters */
	if (maxcnt>48) cnt = (cnt * 48 + maxcnt - 1) / maxcnt;

	/* display histogram */
	for (r=0; r<cnt; r+=1)
	    (void)fputc( '#', stdout);
	(void)fputc( '\n', stdout);
    }
}

static void rstats_print_rtable(rstats_t **rstats_array, size_t count)
{
    size_t r;
    long bad_cnt  = max(1, msgs_bad);
    long good_cnt = max(1, msgs_good);

    assert(bad_cnt > 0);
    assert(good_cnt > 0);

    /* print header */
    if (!Rtable)
	(void)fprintf(stdout, "%*s%6s    %-6s    %-6s    %-6s %s\n",
		      MAXTOKENLEN+2,"","n", "pgood", "pbad", "fw", "U");
    else
	(void)fprintf(stdout, "%*s%6s    %-6s    %-6s    %-6s  %-6s    %-6s %s\n",
		      MAXTOKENLEN+2,"","n", "pgood", "pbad", "fw","invfwlog", "fwlog", "U");

    /* Print 1 line per token */
    for (r= 0; r<count; r+=1)
    {
	rstats_t *cur = rstats_array[r];
	const word_t *token = cur->token;
	int len = max(0, MAXTOKENLEN-(int)token->leng);
	int good = min(cur->good, msgs_good);
	int bad  = min(cur->bad,  msgs_bad);
	double fw = calc_prob(good, bad);
	char flag = (fabs(fw-EVEN_ODDS) - min_dev >= EPS) ? '+' : '-';

	assert(good >= 0 && bad >= 0);

	(void)fputc( '"', stdout);
	(void)word_puts(token, 0, stdout);

	(void)fprintf(stdout, "\"%*s %5d  %8.6f  %8.6f  %8.6f",
		      len, " ", good + bad,
		      (double) good / good_cnt,
		      (double) bad  / bad_cnt,
		      fw);
	if (Rtable)
	    (void)fprintf(stdout, "%10.5f%10.5f",
			  log(1.0 - fw), log(fw));
	(void)fprintf(stdout, " %c\n", flag);
    }

    /* print trailer */
    msg_print_summary();
}
