/* $Id$ */

/*****************************************************************************

NAME:
   graham.c -- code for implementing graham algorithm for computing spamicity.

******************************************************************************/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <config.h>
#include "common.h"

#include "bogofilter.h"
#include "datastore.h"
#include "graham.h"
#include "wordhash.h"

/* constants for the Graham formula */
#define KEEPERS		15		/* how many extrema to keep */
#define MINIMUM_FREQ	5		/* minimum freq */

#define MAX_PROB	0.99f		/* max probability value used */
#define MIN_PROB	0.01f		/* min probability value used */

#define GRAHAM_MIN_DEV		0.4f	/* look for characteristic words */
#define GRAHAM_SPAM_CUTOFF	0.90f	/* if it's spammier than this... */
#define GRAHAM_MAX_REPEATS	4	/* cap on word frequency per message */

int	thresh_index = 0;

/*
** Define a struct so stats can be saved for printing.
*/

stats_t gra_stats;

static const parm_desc gra_parm_table[] =
{
    { "thresh_index",	  CP_INTEGER,	{ (void *) &thresh_index } },
    { NULL,		  CP_NONE,	{ (void *) NULL } },
};

typedef struct 
{
    /*@unique@*/ 
    word_t	*key;
    double	prob; /* WARNING: DBL_EPSILON IN USE, DON'T CHANGE */
} discrim_t;

struct bogostat_s
{
    discrim_t extrema[KEEPERS];
};

#define SIZEOF(array)	((size_t)(sizeof(array)/sizeof(array[0])))

static bogostat_t bogostats;

/* Function Prototypes */

static	void	gra_initialize_constants(void);
static	double	gra_bogofilter(wordhash_t *wordhash, FILE *fp); /*@globals errno@*/
static	double	gra_compute_spamicity(bogostat_t *bs, FILE *fp); /*@globals errno@*/
static	void	gra_print_stats(FILE *fp);
static	double	gra_spamicity(void);
static	rc_t	gra_status(void);
static	void	gra_cleanup(void);

method_t graham_method = {
    "graham",				/* const char		  *name;		*/
    gra_parm_table,	 		/* m_parm_table		  *parm_table		*/
    gra_initialize_constants, 		/* m_initialize_constants *initialize_constants	*/
    gra_bogofilter,	 		/* m_compute_spamicity	  *compute_spamicity	*/
    gra_spamicity,			/* m_spamicity		  *spamicity		*/
    gra_status,				/* m_status		  *status		*/
    gra_print_stats,			/* m_print_bogostats	  *print_stats		*/
    gra_cleanup 			/* m_free		  *cleanup		*/
} ;

/* Function Definitions */

static int compare_extrema(const void *id1, const void *id2)
{
    const discrim_t *d1 = id1;
    const discrim_t *d2 = id2;

    if (d1->prob - d2->prob > EPS) return 1;
    if (d2->prob - d1->prob > EPS) return -1;

    return word_cmp(d1->key, d2->key);
}

static void init_bogostats(/*@out@*/ bogostat_t *bs)
{
    size_t idx;

    for (idx = 0; idx < SIZEOF(bs->extrema); idx++)
    {
	discrim_t *pp = &bs->extrema[idx];
	pp->prob = EVEN_ODDS;
	pp->key = word_new(NULL, 0);
    }
}

static void populate_bogostats(/*@out@*/ bogostat_t *bs,
			       const word_t *word, double prob)
/* if  the new word,prob pair is a better indicator.
 * add them to the bogostats structure */
{
    size_t idx;
    double dev;
    double slotdev, hitdev;
    discrim_t *pp, *hit;

    /* update the list of tokens with maximum deviation */
    dev = DEVIATION(prob);
    hit = NULL;
    hitdev=slotdev=1;

    for (idx = 0; idx < SIZEOF(bs->extrema); idx++)
    {
	pp = &bs->extrema[idx];
	if (pp->key->leng == 0)
	{
	    hit = pp;
	    break;
	} else {
	    slotdev=DEVIATION(pp->prob);
	    if (dev-slotdev>EPS && hitdev-slotdev>EPS)
	    {
		hit=pp;
		hitdev=slotdev;
	    }
	}
    }
    if (hit) 
    { 
	if (verbose >= 3)
	{
	    int i = (hit - bs->extrema);
	    const word_t *key = hit->key;
	    FILE *fp = stderr;	/* dbgout */
	    (void) fprintf(fp,
			   "#  %2d"
			   "  %f  %f  ", i,
			   DEVIATION(prob),  prob);
	    /* print token (max width=20) */
	    (void) word_puts(word, 20, fp);
	    (void) fprintf(fp, "   %f  %f  ",
			   DEVIATION(hit->prob), hit->prob);
	    /* print token (max width=20) */
	    (void) word_puts(key, min(20,key->leng), fp);
	    (void) fputc('\n', fp);
	}
	hit->prob = prob;
	if (word->leng > MAXTOKENLEN) {
	    /* The lexer should not have returned a token longer than
	     * MAXTOKENLEN */
	    internal_error;
	    abort();
	}
	if (hit->key)
	    word_free(hit->key);
	hit->key = word_dup(word);
    }
    return;
}

void gra_print_stats(FILE *fp)
{
    int idx = (thresh_index >= 0) ? thresh_index : KEEPERS+thresh_index;
    discrim_t *pp = &bogostats.extrema[idx];
    if (force || pp->prob >= thresh_stats)
	(void)gra_compute_spamicity( &bogostats, fp );
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
    double count = wordstats->good + wordstats->bad;
    double prob = wordstats->bad/count;

    return (prob);
}

static double compute_probability(const word_t *token)
{
    wordlist_t* list;
    int override=0;
    long count;
    double prob;
    int totalcount=0;

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
	    totalcount+=count*list->weight;
	    override=list->override;
	    prob = (double)count;
	    prob /= list->msgcount;
	    prob *= list->weight;
	    prob = min(1.0, prob);
	    
	    wordprob_add(&wordstats, prob, list->bad);
	}
    }

    if (totalcount < MINIMUM_FREQ) {
	prob=UNKNOWN_WORD;
    } else {
	prob=wordprob_result(&wordstats);
	prob = min(MAX_PROB, prob);
	prob = max(MIN_PROB, prob);
    }

    return prob;
}

static bogostat_t *select_indicators(wordhash_t *wordhash)
/* selects the best spam/nonspam indicators and
 * populates the bogostats structure.
 */
{
    int w_count = 0;
    hashnode_t *node;

    init_bogostats(&bogostats);

    wordhash_sort(wordhash);

    for(node = wordhash_first(wordhash); node != NULL; node = wordhash_next(wordhash))
    {
	word_t *token = node->key;
	double prob = compute_probability( token );

	w_count += 1;
	populate_bogostats( &bogostats, token, prob );
    }

    return (&bogostats);
}

double gra_compute_spamicity(bogostat_t *bs, FILE *fp) /*@globals errno@*/
/* computes the spamicity of the words in the bogostat structure
 * returns:  the spamicity */
{
    double product, invproduct;
    double spamicity = 0.0;

    discrim_t *pp;

    if (verbose)
    {
	/* put the bs in ascending order by probability and alphabet */
	qsort(bs->extrema, SIZEOF(bs->extrema), sizeof(discrim_t), compare_extrema);
    }

    /* Bayes' theorem. */
    /* For discussion, see <http://www.mathpages.com/home/kmath267.htm>. */
    product = invproduct = 1.0f;
    for (pp = bs->extrema; pp < bs->extrema+SIZEOF(bs->extrema); pp++)
    {
	if (fabs(pp->prob) < EPS)
	    continue;

	product *= pp->prob;
	invproduct *= (1 - pp->prob);
	spamicity = product / (product + invproduct);

	if (fp != NULL && verbose > 1)
	{
	    fprintf(fp, "%s%f  ", stats_prefix, pp->prob);
	    if (verbose == 3)
		fprintf(fp, "%f  ", spamicity);
	    if (verbose >= 4)
		fprintf(fp, "%f  %f  %8.5e  ", product, invproduct, spamicity);
	    fwrite(pp->key->text, 1, pp->key->leng, fp);
	    fputc('\n', fp);
	}
    }

    return spamicity;
}

void gra_initialize_constants(void)
{
    mth_initialize( &gra_stats, GRAHAM_MAX_REPEATS, GRAHAM_MIN_DEV, GRAHAM_SPAM_CUTOFF, GRAHAM_GOOD_BIAS );
}

double gra_spamicity(void)
{
    return gra_stats.spamicity;
}

rc_t gra_status(void)
{
    rc_t status = ( gra_stats.spamicity >= spam_cutoff ) ? RC_SPAM : RC_HAM;
    return status;
}

double gra_bogofilter(wordhash_t *wordhash, FILE *fp) /*@globals errno@*/
{
    bogostat_t	*bs = select_indicators(wordhash);    /* select the best spam/nonspam indicators. */

    gra_stats.spamicity = gra_compute_spamicity(bs, fp);

    return gra_stats.spamicity;
}

void gra_cleanup(void)
{
}

/* Done */
