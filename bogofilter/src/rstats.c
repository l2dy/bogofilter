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

extern int Rtable;
extern double min_dev;

extern	long msgs_good;		/* from robinson.c */
extern	long msgs_bad;		/* from robinson.c */

extern	double	robx;		/* from robinson.c */
extern	double	robs;		/* from robinson.c */

typedef struct rstats_s rstats_t;
struct rstats_s {
    rstats_t *next;
    word_t *token;
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
    size_t    count;		/* words in list */
    size_t    robn;		/* words in score */
    FLOAT     p;		/* Robinson's P */
    FLOAT     q;		/* Robinson's Q */
    double    spamicity;
};

static header_t  header;
static rstats_t *current = NULL;

void rstats_print_histogram(size_t robn, rstats_t **rstats_array, size_t count);
void rstats_print_rtable(rstats_t **rstats_array, size_t count);
void rstats_print_rtable_summary(void);

void rstats_init(void)
{
    header.list = (rstats_t *) xcalloc( 1, sizeof(rstats_t));
    current = header.list;
}

void rstats_cleanup(void)
{
    rstats_t *p, *q;
    
    for (p = current; p != NULL; p = q)
    {
      q = p->next;
      xfree (p);
    }
}

void rstats_add( const word_t *token,
		 double good,
		 double bad,
		 double prob)
{
    header.count += 1;
    current->next  = NULL;
    current->token = word_dup(token);
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

void rstats_print(void)
{
    size_t r;
    size_t robn  = header.robn;
    size_t count = header.count;
    rstats_t *cur;
    rstats_t **rstats_array = (rstats_t **) xcalloc(count, sizeof(rstats_t *));

    for (r= 0, cur = header.list; r<count; r+=1, cur=cur->next)
	rstats_array[r] = cur;

    /* sort by ascending probability, then name */
    qsort(rstats_array, count, sizeof(rstats_t *), compare_rstats_t);

    if (Rtable || verbose>=3)
	rstats_print_rtable(rstats_array, count);
    else
	if (verbose==2)
	    rstats_print_histogram(robn, rstats_array, count);

    for (r= 0; r<count; r+=1)
    {
	cur = rstats_array[r];
	xfree(cur->token);
	xfree(cur);
    }

    xfree(rstats_array);
}

void rstats_print_histogram(size_t robn, rstats_t **rstats_array, size_t count)
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
	while (r < count)
	{
	    double prob = rstats_array[r]->prob;
	    if (prob - fin >= EPS)
		break;

	    if (fabs(EVEN_ODDS - prob) - min_dev >= EPS)
	    {
		cnt += 1;
		h->prob += prob;
		invlogsum += log(1.0 - prob);
		logsum += log(prob);
	    }

	    if (robn == 0)
	    {
		h->spamicity = robx;
	    }
	    else
	    {
		double invn, invproduct, product;
		invn = (double)robn;
		invproduct = 1.0 - exp(invlogsum / invn);
		product = 1.0 - exp(logsum / invn);
		h->spamicity =
		    (1.0 + (invproduct - product) / (invproduct + product)) / 2.0;
	    }
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
	(void)fprintf(stdout, "%s%5.2f %4lu  %f  %f  ", stats_prefix, beg, (unsigned long)cnt, prob, h->spamicity );

	/* scale histogram to 50 characters */
	if (maxcnt>50) cnt = (cnt * 50 + maxcnt - 1) / maxcnt;

	/* display histogram */
	for (r=0; r<cnt; r+=1)
	    (void)fputc( '#', stdout);
	(void)fputc( '\n', stdout);
    }
}

void rstats_print_rtable(rstats_t **rstats_array, size_t count)
{
    size_t r;
    long bad_cnt  = max(1, msgs_bad);
    long good_cnt = max(1, msgs_good);

    /* print header */
    if (!Rtable)
	(void)fprintf(stdout, "%*s%6s    %-6s    %-6s    %-6s %s\n",
		      MAXTOKENLEN+2,"","n", "pgood", "pbad", "fw", "U");
    else
	(void)fprintf(stdout, "%*s%6s    %-6s    %-6s    %-6s    %-6s    %-6s %s\n",
		      MAXTOKENLEN+2,"","n", "pgood", "pbad", "fw","invfwlog", "fwlog", "U");

    /* Print 1 line per token */
    for (r= 0; r<count; r+=1)
    {
	rstats_t *cur = rstats_array[r];
	const word_t *token = cur->token;
	int len = max(0, MAXTOKENLEN-(int)token->leng);
	double g = min(cur->good, msgs_good);
	double b = min(cur->bad,  msgs_bad);
	double c = g + b;
	int n = c;
	double pw = ((n == 0) 
		     ? 0.0
		     : ((b / bad_cnt) /
			(b / bad_cnt + g / good_cnt)));
	double fw = (robs * robx + c * pw) / (robs + c);
	char flag = (fabs(fw-EVEN_ODDS) - min_dev >= EPS) ? '+' : '-';

	(void)fputc( '"', stdout);
	(void)word_puts(token, 0, stdout);

	if (!Rtable)
	    (void)fprintf(stdout, "\"%*s %5d  %8.6f  %8.6f  %8.6f %c\n",
			  len, " ",
			  n, g / good_cnt, b / bad_cnt, fw, flag);
	else
	    (void)fprintf(stdout, "\"%*s %5d  %8.6f  %8.6f  %8.6f%10.5f%10.5f %c\n",
			  len, " ",
			  n, g / good_cnt, b / bad_cnt,
			  fw, log(1.0 - fw), log(fw), flag);
    }

    /* print trailer */
    ((rf_method_t *)method)->print_summary();
}
