/* $Id$ */

/*****************************************************************************

NAME:
   rstats.c -- routines for printing robinson data for debugging.

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <config.h>
#include "common.h"

#include "bogofilter.h"
#include "robinson.h"
#include "rstats.h"
#include "xmalloc.h"
#include "xstrdup.h"

extern int Rtable;
extern double min_dev;

typedef struct rstats_s rstats_t;
struct rstats_s {
    rstats_t *next;
    char *token;
    double good;
    double bad;
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
    size_t    robn;
    FLOAT     p;		/* Robinson's P */
    FLOAT     q;		/* Robinson's Q */
    double    spamicity;
};

static size_t	 count;
static header_t  header;
static rstats_t *current = NULL;

void rstats_print_histogram(size_t robn, rstats_t **rstats_array);
void rstats_print_rtable(size_t robn, rstats_t **rstats_array);
void rstats_print_rtable_summary(void);

void rstats_init(void)
{
    header.list = (rstats_t *) xcalloc( 1, sizeof(rstats_t));
    current = header.list;
}

void rstats_add( const char *token,
		 double good,
		 double bad,
		 double prob)
{
    count += 1;
    current->next  = NULL;
    current->token = xstrdup(token);
    current->good  = good;
    current->bad   = bad;
    current->prob  = prob;
    current->next = (rstats_t *)xmalloc(sizeof(rstats_t));
    current = current->next;
}

static int compare_rstats_t(const void *const ir1, const void *const ir2)
{
    const rstats_t *r1 = *(const rstats_t *const *)ir1;
    const rstats_t *r2 = *(const rstats_t *const *)ir2;

    if (r1->prob > r2->prob) return 1;
    if (r1->prob < r2->prob) return -1;

    return strcmp(r1->token, r2->token);
}

#define	INTERVALS	10

void rstats_fini(size_t robn, FLOAT P, FLOAT Q, double spamicity)
{
    header.robn       = robn;
    header.p          = P;
    header.q          = Q;
    header.spamicity  = spamicity;
}

void rstats_print(void)
{
    size_t r;
    size_t robn = header.robn;
    rstats_t *cur;
    rstats_t **rstats_array = (rstats_t **) xcalloc( robn, sizeof(rstats_t *));

    for (r= 0, cur = header.list; r<robn; r+=1, cur=cur->next)
	rstats_array[r] = cur;

    qsort(rstats_array, robn, sizeof(rstats_t *), compare_rstats_t);

    if (Rtable || verbose>=3)
	rstats_print_rtable(robn, rstats_array);
    else
	if (verbose==2)
	    rstats_print_histogram(robn, rstats_array);

    for (r= 0; r<robn; r+=1)
    {
	cur = rstats_array[r];
	xfree(cur->token);
	xfree(cur);
    }

    xfree(rstats_array);
}

void rstats_print_histogram(size_t robn, rstats_t **rstats_array)
{
    size_t i, r;
    rhistogram_t hist[INTERVALS];
    size_t maxcnt=0;

    double invlogsum = 0.0;	/* Robinson's P */
    double logsum = 0.0;	/* Robinson's Q */

    /* Compute histogram */
    for (i=r=0; i<INTERVALS; i+=1)
    {
	rhistogram_t *h = &hist[i];
	double fin = 1.0*(i+1)/INTERVALS;
	size_t cnt = 0;
	h->prob = 0.0;
	h->spamicity=0.0;
	while ( r<robn)
	{
	    double prob = rstats_array[r]->prob;
	    double invn, invproduct, product, spamicity;
	    if (prob >= fin)
		break;
	    cnt += 1;
	    h->prob += prob;

	    if (fabs(EVEN_ODDS - prob) >= min_dev)
	    {
		invlogsum += log(1.0 - prob);
		logsum += log(prob);
	    }

	    invn = (double)robn;
	    invproduct = 1.0 - exp(invlogsum / invn);
	    product = 1.0 - exp(logsum / invn);
	    spamicity =
		(1.0 + (invproduct - product) / (invproduct + product)) / 2.0;
	    h->spamicity=spamicity;

	    r += 1;
	}
	h->count=cnt;
	maxcnt = max(maxcnt, cnt);
    }

    (void)fprintf(stdout, "%s%5s %4s %7s   %9s  %s\n", stats_prefix, "int", "cnt", "prob", "spamicity", "histogram" );

    /* Print histogram */
    for (i=0; i<INTERVALS; i+=1)
    {
	double beg = 1.0*i/INTERVALS;
	rhistogram_t *h = &hist[i];
	size_t cnt = h->count;
	double prob = cnt ? h->prob/cnt : 0.0;

	/* print interval, count, probability, and spamicity */
	(void)fprintf(stdout, "%s%5.2f %4d  %f  %f  ", stats_prefix, beg, cnt, prob, h->spamicity );

	/* scale histogram to 50 characters */
	if (maxcnt>50) cnt = (cnt * 50 + maxcnt - 1) / maxcnt;

	/* display histogram */
	for (r=0; r<cnt; r+=1)
	    (void)fputc( '#', stdout);
	(void)fputc( '\n', stdout);
    }
}

void rstats_print_rtable(size_t robn, rstats_t **rstats_array)
{
    size_t r;

    /* print header */
    (void)fprintf(stdout, "     %-20s%10s%10s%10s%10s%10s\n",
		  "Token","pgood","pbad","fw","invfwlog","fwlog");

    /* Print 1 line per token */
    for (r= 0; r<robn; r+=1)
    {
	rstats_t *cur = rstats_array[r];
	double prob = cur->prob;
	char flag = (fabs(prob-EVEN_ODDS) < min_dev) ? '-' : '+';

	(void)fprintf(stdout, "%3d  %-20s  %8.2f  %8.0f  %8.6f  %8.5f  %8.5f %c\n",
		      r+1, cur->token, cur->good, cur->bad, prob, 
		      log(1.0 - prob), log(prob), flag);
    }

    /* print trailer */
    ((rf_method_t *)method)->print_summary();
}
