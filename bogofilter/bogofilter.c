/* $Id$ */

/*****************************************************************************

NAME:
   bogofilter.c -- detect spam and bogons presented on standard input.

AUTHOR:
   Eric S. Raymond <esr@thyrsus.com>

THEORY:
   This is Paul Graham's variant of Bayes filtering described at 

	http://www.paulgraham.com/spam.html

I do the lexical analysis slightly differently, however.

MOD: (Greg Louis <glouis@dynamicro.on.ca>) This version implements Gary
    Robinson's proposed modifications to the "spamicity" calculation and
    uses his f(w) individual probability calculation.  See

    http://radio.weblogs.com/0101454/stories/2002/09/16/spamDetection.html
    
    In addition, this version does not store "extrema."  Instead it accumulates
    Robinson's P and Q using all words deemed "characteristic," i.e. having
    a deviation (fabs (0.5f - prob)) >= MIN_DEV, currently set to 0.0.

******************************************************************************/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "common.h"
#include "bogofilter.h"
#include "datastore.h"
#include "lexer.h"
#include "rstats.h"
#include "wordhash.h"

/* constants for the Graham formula */
#define KEEPERS		15		/* how many extrema to keep */
#define MINIMUM_FREQ	5		/* minimum freq */

#define MAX_PROB	0.99f		/* max probability value used */
#define MIN_PROB	0.01f		/* min probability value used */
#define DEVIATION(n)	fabs((n) - EVEN_ODDS)	/* deviation from average */

#define GRAHAM_MIN_DEV		0.4f	/* look for characteristic words */
#define ROBINSON_MIN_DEV	0.0f	/* if nonzero, use characteristic words */

#define GRAHAM_SPAM_CUTOFF	0.90f	/* if it's spammier than this... */
#define ROBINSON_SPAM_CUTOFF	0.54f	/* if it's spammier than this... */

#define GRAHAM_MAX_REPEATS	4	/* cap on word frequency per message */
#define ROBINSON_MAX_REPEATS	1	/* cap on word frequency per message */

int max_repeats;
double spam_cutoff;
extern double min_dev;
extern double robs;
extern double robx;

#define PLURAL(count) ((count == 1) ? "" : "s")

extern char msg_register[];
extern int Rtable;
static double scalefactor;
extern char *stats_prefix;

static void initialize_constants(void);

static void wordprop_init(void *vwordprop){
	wordprop_t *wordprop = vwordprop;

	wordprop->freq = 0;
	wordprop->msg_freq = 0;
}

static void *collect_words(/*@unused@*/ int fd, /*@out@*/ int *message_count,
	/*@out@*/ int *word_count)
    /* tokenize input text and save words in wordhash_t hash table 
     * returns: the wordhash_t hash table.
     * Sets messageg_count and word_count to the appropriate values
     * if their pointers are non-NULL.  */
{
  int w_count = 0;
  int msg_count = 0;
 
  wordprop_t *w;
  hashnode_t *n;
  wordhash_t *h = wordhash_init();
     
  for (;;){
    token_t tok = get_token();
  
    if (tok != FROM && tok != 0){
      w = wordhash_insert(h, yylval, sizeof(wordprop_t), &wordprop_init);
      w->msg_freq++;
      w_count++;
    }
    else {
      /* End of message. Update message counts. */
      if (tok == FROM || (tok == 0 && msg_count == 0))
        msg_count++;
  
      /* Incremenent word frequencies, capping each message's
       * contribution at MAX_REPEATS in order to be able to cap
       * frequencies. */
      for(n = wordhash_first(h); n != NULL; n = wordhash_next(h)){
        w = n->buf;
        if (w->msg_freq > max_repeats)
          w->msg_freq = max_repeats;
  
        w->freq += w->msg_freq;
        w->msg_freq = 0;
      }
  
      /* Want to process EOF, *then* drop out */
      if (tok == 0)
        break;
    }
  }
 
  if (word_count)
    *word_count = w_count;
 
  if (message_count)
    *message_count = msg_count;
 
  return(h);
}


static void register_words(run_t run_type, wordhash_t *h,
	int msgcount, int wordcount)
/* tokenize text on stdin and register it to  a specified list
 * and possibly out of another list
 */
{
  char ch = '\0';
  hashnode_t *node;
  wordprop_t *wordprop;

  wordlist_t *list;
  wordlist_t *incr_list = NULL;
  wordlist_t *decr_list = NULL;

  switch(run_type)
  {
  case REG_SPAM:		ch = 's' ;  break;
  case REG_GOOD:		ch = 'n' ;  break;
  case REG_GOOD_TO_SPAM:	ch = 'S' ;  break;
  case REG_SPAM_TO_GOOD:	ch = 'N' ;  break;
  default:			abort(); 
  }

  (void)sprintf(msg_register, "register-%c, %d words, %d messages\n", ch,
	  wordcount, msgcount);

  if (verbose)
    (void)fprintf(stderr, "# %d word%s, %d message%s\n", 
	    wordcount, PLURAL(wordcount), msgcount, PLURAL(msgcount));

  good_list.active = spam_list.active = FALSE;

  switch(run_type)
    {
    case REG_GOOD:
      incr_list = &good_list;
      break;

    case REG_SPAM:
      incr_list = &spam_list;
      break;

    case  REG_GOOD_TO_SPAM:
      decr_list = &good_list;
      incr_list = &spam_list;
      break;

    case REG_SPAM_TO_GOOD:
      incr_list = &good_list;
      decr_list = &spam_list;
      break;
     
    default:
      (void)fprintf(stderr, "Error: Invalid run_type\n");
      exit(2);      
    }

  incr_list->active = TRUE;
  if (decr_list)
    decr_list->active = TRUE;

  db_lock_writer_list(word_lists);

  for (list = word_lists; list != NULL; list = list->next){
    if (list->active == TRUE) {
      list->msgcount = db_getcount(list->dbh);
    }
  }
 
  incr_list->msgcount += msgcount;

  if (decr_list){
    if (decr_list->msgcount > msgcount)
      decr_list->msgcount -= msgcount;
    else
      decr_list->msgcount = 0;
  }

  for (node = wordhash_first(h); node != NULL; node = wordhash_next(h)){
    wordprop = node->buf;
    db_increment(incr_list->dbh, node->key, wordprop->freq);
    if (decr_list) db_decrement(decr_list->dbh, node->key, wordprop->freq);
  }

  for (list = word_lists; list != NULL; list = list->next){
    if (list->active) {
      db_setcount(list->dbh, list->msgcount);
      db_flush(list->dbh);
      if (verbose>1)
	(void)fprintf(stderr, "bogofilter: %ld messages on the %s list\n",
		list->msgcount, list->name);
    }
  }

  db_lock_release_list(word_lists);
}

void register_messages(int fdin, run_t run_type)
{
  wordhash_t *h;
  int	wordcount, msgcount;
  initialize_constants();
  h = collect_words(fdin, &msgcount, &wordcount);
  register_words(run_type, h, msgcount, wordcount);
  wordhash_free(h);
}

typedef struct 
{
    /*@unique@*/ char        key[MAXTOKENLEN+1];
    double      prob;
}
discrim_t;

struct bogostat_s
{
    discrim_t extrema[KEEPERS];
};

#define SIZEOF(array)	((size_t)(sizeof(array)/sizeof(array[0])))

typedef struct bogostat_s bogostat_t;
static bogostat_t bogostats;

static double compute_spamicity(bogostat_t *bogostats, /*@null@*/ FILE *fp);

static int compare_extrema(const void *id1, const void *id2)
{ 
    const discrim_t *d1 = id1;
    const discrim_t *d2 = id2;
    return ( (d1->prob > d2->prob) ||
	     ((d1->prob == d2->prob) && (strcmp(d1->key, d2->key) > 0)));
}

static void init_bogostats(/*@out@*/ bogostat_t *bogostats)
{
    size_t idx;

    for (idx = 0; idx < SIZEOF(bogostats->extrema); idx++)
    {
	discrim_t *pp = &bogostats->extrema[idx];
	pp->prob = EVEN_ODDS;
	pp->key[0] = '\0';
    }
}

static void populate_bogostats(/*@out@*/ bogostat_t *bogostats,
	const char *text, double prob,
	/*@unused@*/ int count)
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
    hitdev=1;

    for (idx = 0; idx < SIZEOF(bogostats->extrema); idx++)
    {
	pp = &bogostats->extrema[idx];
	if (pp->key[0] == '\0' )
	{
	    hit = pp;
	    break;
	}
	else
	{
	    slotdev=DEVIATION(pp->prob);

	    if (dev>slotdev && hitdev>slotdev)
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
	    int i = (hit - bogostats->extrema);
	    char *curkey = hit->key[0] ? hit->key : "";
	    (void)fprintf(stderr, 
		    "#  %2d"
		    "  %f  %f  %-20.20s "
		    "  %f  %f  %s\n", i,
		   DEVIATION(prob),  prob, text,
		   DEVIATION(hit->prob), hit->prob, curkey);
	}
	hit->prob = prob;
	strncpy(hit->key, text, MAXTOKENLEN);
	hit->key[MAXTOKENLEN] = '\0';
    }
}

void print_bogostats(FILE *fp, double spamicity)
{
    extern int thresh_index;
    extern double thresh_stats;
    extern double thresh_rtable;

    switch(algorithm) {
    case AL_GRAHAM:
    {
	int index = (thresh_index >= 0) ? thresh_index : KEEPERS+thresh_index;
	discrim_t *pp = &bogostats.extrema[index];
	if (pp->prob >= thresh_stats)
	    (void)compute_spamicity(&bogostats, fp);
	break;
    }
    case AL_ROBINSON:
	if (spamicity > thresh_stats || spamicity > thresh_rtable)
	    rstats_print();
	break;
    default:
	abort();
    }
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
    double prob;
    double count = wordstats->good + wordstats->bad;

    switch(algorithm) {
	case AL_GRAHAM:
	    prob = wordstats->bad/count;
	    break;

	case AL_ROBINSON:
	    prob = ((ROBS * ROBX + wordstats->bad) / (ROBS + count));
	    break;

	default:
	    abort();
    }

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
    int totalcount=0;
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
	    totalcount+=count*list->weight;
	    override=list->override;
	    prob = (double)count;

	    switch (algorithm)
	    {
	    case AL_GRAHAM:
 	        prob /= list->msgcount;
 	        prob *= list->weight;
		prob = min(1.0, prob);
		break;

	    case AL_ROBINSON:
 	        if (!list->bad)
		    prob *= scalefactor;
		break;

	    default:
		abort();
	    }

	    wordprob_add(&wordstats, prob, list->bad);
	}
    }

    switch(algorithm) {
    case AL_GRAHAM:
	if (totalcount < MINIMUM_FREQ) {
	    prob=UNKNOWN_WORD;
	} else {
	    prob=wordprob_result(&wordstats);
	    prob = min(MAX_PROB, prob);
	    prob = max(MIN_PROB, prob);
	}
	break;

    case AL_ROBINSON:
	prob=wordprob_result(&wordstats);
	if (Rtable || verbose)
	    rstats_add(token, wordstats.good, wordstats.bad, prob);
	break;

    default:
	abort();
    }

    return prob;
}

static bogostat_t *select_indicators(wordhash_t *wordhash)
/* selects the best spam/nonspam indicators and
 * populates the bogostats structure.
 */
{
    hashnode_t *node;

    init_bogostats(&bogostats);

    for(node = wordhash_first(wordhash); node != NULL; node = wordhash_next(wordhash))
    {
	char *token = node->key;
	double prob = compute_probability( token );

	populate_bogostats( &bogostats, token, prob, 1 );
    }

    return (&bogostats);
}

static double compute_spamicity(bogostat_t *bogostats, FILE *fp) /*@globals errno@*/
/* computes the spamicity of the words in the bogostat structure
 * returns:  the spamicity */
{
    double product, invproduct;
    double spamicity = 0.0;

    discrim_t *pp;

    if (verbose)
    {
	/* put the bogostats in ascending order by probability and alphabet */
	qsort(bogostats->extrema, SIZEOF(bogostats->extrema), sizeof(discrim_t), compare_extrema);
    }

    /* Bayes' theorem. */
    /* For discussion, see <http://www.mathpages.com/home/kmath267.htm>. */
    product = invproduct = 1.0f;
    for (pp = bogostats->extrema; pp < bogostats->extrema+SIZEOF(bogostats->extrema); pp++)
    {
	if (pp->prob == 0)
	    continue;

	product *= pp->prob;
	invproduct *= (1 - pp->prob);
	spamicity = product / (product + invproduct);

	if (fp != NULL)
	{
	    switch (verbose)
	    {
		case 0:
		case 1:
		    break;
		case 2:
		    fprintf(fp, "%s%f  %s\n", stats_prefix, pp->prob, pp->key);
		    break;
		case 3:
		    fprintf(fp, "%s%f  %f  %s\n", stats_prefix, pp->prob, spamicity, pp->key);
		    break;
		default:
		    fprintf(fp, "%s%f  %f  %f  %8.5e  %s\n", stats_prefix, pp->prob, product, invproduct, spamicity, pp->key);
		    break;
	    }
	}
    }

    return spamicity;
}

static double compute_robinson_spamicity(wordhash_t *wordhash) /*@globals errno@*/
/* selects the best spam/nonspam indicators and calculates Robinson's S */
{
    hashnode_t *node;

    double invlogsum = 0.0;	/* Robinson's P */
    double logsum = 0.0;	/* Robinson's Q */
    double spamicity;
    int robn = 0;

    if (robx == 0.0f)		/* if not yet set ... */
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

static void initialize_constants(void)
{
    switch(algorithm) {
	case AL_GRAHAM:
	    spam_cutoff = GRAHAM_SPAM_CUTOFF;
	    max_repeats = GRAHAM_MAX_REPEATS;
	    if ( min_dev == 0.0f )		/* if not yet set ... */
		min_dev     = GRAHAM_MIN_DEV;
	    break;

	case AL_ROBINSON:
	    spam_cutoff = ROBINSON_SPAM_CUTOFF;
	    max_repeats = ROBINSON_MAX_REPEATS;
	    scalefactor = compute_scale();
	    if ( min_dev == 0.0f )		/* if not yet set ... */
		min_dev     = ROBINSON_MIN_DEV;
	    if ( robs == 0.0f )			/* if not yet set ... */
		robs = ROBS;
	    break;

	default:
	    abort();
    }
}

rc_t bogofilter(int fd, double *xss) /*@globals errno@*/
/* evaluate text for spamicity */
{
    rc_t	status;
    double 	spamicity;
    wordhash_t  *wordhash;
    bogostat_t	*bogostats;
    int		wordcount, msgcount;

    /* tokenize input text and save words in a wordhash. */
    wordhash = collect_words(fd, &msgcount, &wordcount);

    good_list.active = spam_list.active = TRUE;

    db_lock_reader_list(word_lists);

    good_list.msgcount = db_getcount(good_list.dbh);
    spam_list.msgcount = db_getcount(spam_list.dbh);

    initialize_constants();

    switch(algorithm) {
	case AL_GRAHAM:
	    /* select the best spam/nonspam indicators. */
	    bogostats = select_indicators(wordhash);

	    /* computes the spamicity of the spam/nonspam indicators. */
	    spamicity = compute_spamicity(bogostats, NULL);
	    break;

	case AL_ROBINSON:
	    /* computes the spamicity of the spam/nonspam indicators. */
	    spamicity = compute_robinson_spamicity(wordhash);
	    break;

	default:
	    abort();
    }

    db_lock_release_list(word_lists);

    status = (spamicity > spam_cutoff) ? RC_SPAM : RC_NONSPAM;

    if (xss != NULL)
        *xss = spamicity;

    if (run_type == RUN_UPDATE)
      register_words((status==RC_SPAM) ? REG_SPAM : REG_GOOD, wordhash,
	      msgcount, wordcount);

    wordhash_free(wordhash);

    return status;
}

/* Done */
