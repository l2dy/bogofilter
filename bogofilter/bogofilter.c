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

******************************************************************************/
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>

#include "bogofilter.h"
#include "datastore.h"
#include "wordhash.h"
#include "common.h"

// constants for the Graham formula 
#define KEEPERS		15		// how many extrema to keep
#define MINIMUM_FREQ	5		// minimum freq
#define UNKNOWN_WORD	0.4f		// odds that unknown word is spammish
#define SPAM_CUTOFF	0.9f		// if it's spammier than this...
#define MAX_REPEATS	4		// cap on word frequency per message

#define MAX_PROB	0.99f		// max probability value used
#define MIN_PROB	0.01f		// min probability value used
#define EVEN_ODDS	0.5f		// used for words we want to ignore
#define DEVIATION(n)	fabs((n) - EVEN_ODDS)		// deviation from average

extern char msg_register[];

static void wordprop_init(void *vwordprop){
	wordprop_t *wordprop = vwordprop;

	wordprop->freq = 0;
	wordprop->msg_freq = 0;
}

void *collect_words(int fd, int *msg_count, int *word_count)
// tokenize input text and save words in wordhash_t hash table
// returns: the wordhash_t hash table. Sets msg_count and word_count to the appropriate values
{
  int tok = 0;
  int w_count = 0;
  int mm_count = 0;
 
  wordprop_t *w;
  hashnode_t *n;
  wordhash_t *h = wordhash_init();
     
  for (;;){
    tok = get_token();
  
    if (tok != FROM && tok != 0){
      w = wordhash_insert(h, yylval, sizeof(wordprop_t), &wordprop_init);
      w->msg_freq++;
      w_count++;
    }
    else {
      // End of message. Update message counts.
      if (tok == FROM || (tok == 0 && mm_count == 0))
        mm_count++;
  
      // Incremenent word frequencies, capping each message's contribution at MAX_REPEATS
      // in order to be able to cap frequencies.
      for(n = wordhash_first(h); n != NULL; n = wordhash_next(h)){
        w = n->buf;
        if (w->msg_freq > MAX_REPEATS)
          w->msg_freq = MAX_REPEATS;
  
        w->freq += w->msg_freq;
        w->msg_freq = 0;
      }
  
      // Want to process EOF, *then* drop out
      if (tok == 0)
        break;
    }
  }
 
  if (word_count)
    *word_count = w_count;
 
  if (msg_count)
    *msg_count = mm_count;
 
  return(h);
}


void register_words(reg_t register_type, wordhash_t *h, int msgcount, int wordcount)
// tokenize text on stdin and register it to  a specified list
// and possibly out of another list
{
  char ch;
  hashnode_t *node;
  wordprop_t *wordprop;

  wordlist_t *list;
  wordlist_t *incr_list = NULL;
  wordlist_t *decr_list = NULL;

  switch(register_type)
  {
  case REG_SPAM:		ch = 's' ; break;
  case REG_GOOD:		ch = 'n' ; break;
  case REG_GOOD_TO_SPAM:	ch = 'S' ; break;
  case REG_SPAM_TO_GOOD:	ch = 'N' ; break;
  }

  sprintf(msg_register, "register-%c, %d words, %d messages\n", ch, wordcount, msgcount);

  if (verbose)
    fprintf(stderr, "# %d words, %d messages\n", wordcount, msgcount);

  good_list.active = spam_list.active = FALSE;

  switch(register_type)
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
      fprintf(stderr, "Error: Invalid register_type\n");
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
    if (decr_list) db_increment(decr_list->dbh, node->key, -wordprop->freq);
  }

  for (list = word_lists; list != NULL; list = list->next){
    if (list->active) {
      db_setcount(list->dbh, list->msgcount);
      db_flush(list->dbh);
      if (verbose)
	fprintf(stderr, "bogofilter: %lu messages on the %s list\n", list->msgcount, list->name);
    }
  }

  db_lock_release_list(word_lists);
}

void register_messages(int fdin, reg_t register_type)
{
  wordhash_t *h;
  int	wordcount, msgcount;
  h = collect_words(fdin, &msgcount, &wordcount);
  register_words(register_type, h, msgcount, wordcount);
  wordhash_free(h);
}

typedef struct 
{
    char        key[MAXTOKENLEN+1];
    double      prob;
}
discrim_t;

typedef struct
{
    discrim_t extrema[KEEPERS];
}
bogostat_t;

#define SIZEOF(array)	((size_t)(sizeof(array)/sizeof(array[0])))

static bogostat_t stats;

int compare_stats(const void *id1, const void *id2)
{ 
    const discrim_t *d1 = id1;
    const discrim_t *d2 = id2;
    return ( (d1->prob > d2->prob) ||
	     ((d1->prob == d2->prob) && (strcmp(d1->key, d2->key) > 0)));
}

void init_stats(bogostat_t *stats)
{
    int idx;

    for (idx = 0; idx < SIZEOF(stats->extrema); idx++)
    {
	discrim_t *pp = &stats->extrema[idx];
	pp->prob = EVEN_ODDS;
	pp->key[0] = '\0';
    }
}

void populate_stats( bogostat_t *stats, char *text, double prob, int count )
// if  the new word,prob pair is a better indicator.
// add them to the stats structure, 
{
    size_t idx;
    double dev;
    double slotdev, hitdev;
    discrim_t *pp, *hit;

    // update the list of tokens with maximum deviation
    dev = DEVIATION(prob);
    hit = NULL;
    hitdev=1;

    for (idx = 0; idx < SIZEOF(stats->extrema); idx++)
    {
	pp = &stats->extrema[idx];
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
	hit->prob = prob;
	strncpy(hit->key, text, MAXTOKENLEN);
    }
}

void print_stats( bogostat_t *stats )
{
    size_t idx;
    for (idx = 0; idx < SIZEOF(stats->extrema); idx++)
    {
	discrim_t *pp = &stats->extrema[idx];
	fprintf(stderr, "#  %2ld  %f  %s\n", (long)idx, pp->prob, pp->key);
    }
}

typedef struct {
    double good;
    double bad;
} wordprob_t;

void wordprob_init(wordprob_t* stats)
{
    stats->good=stats->bad=0;
}

void wordprob_add(wordprob_t* stats, double newprob, int bad)
{
    if (bad)
	stats->bad+=newprob;
    else
	stats->good+=newprob;
}

double wordprob_result(wordprob_t* stats)
{
    double prob = stats->bad/(stats->good + stats->bad);
    return (prob);
}

double compute_probability( char *token )
{
    wordlist_t* list;
    int override=0;
    long count;
    int totalcount=0;
    double prob;

    wordprob_t stats;

    wordprob_init(&stats);

    for (list=word_lists; list != NULL ; list=list->next)
    {
	if (verbose >= 2)
	    fprintf(stderr, "checking list %s for word '%s'.\n", list->name, token);
	if (override > list->override) break;
	count=db_getvalue(list->dbh, token);
	if (count) {
	    if (list->ignore)
		return EVEN_ODDS;
	    if (verbose >= 3)
		fprintf(stderr, "word '%s' found on list %s with count %ld.\n", token, list->name, count);
	    totalcount+=count*list->weight;
	    override=list->override;
	    prob = (double)count;
	    prob /= list->msgcount;
	    prob *= list->weight;
	    if (verbose >= 4)
		fprintf(stderr, "word '%s' has uncorrected spamicity %f.\n", token, prob);

	    prob = min(1.0, prob);

	    if (verbose >= 4)
		fprintf(stderr, "word '%s' has spamicity %f.\n", token, prob);

	    wordprob_add(&stats, prob, list->bad);
	}
    }
    if (totalcount < MINIMUM_FREQ)
	prob=UNKNOWN_WORD;
    else {
	prob=wordprob_result(&stats);
	prob = min(MAX_PROB, prob);
	prob = max(MIN_PROB, prob);
    }
    return prob;
}

bogostat_t *select_indicators(wordhash_t *wordhash)
// selects the best spam/nonspam indicators and
// populates the stats structure.
{
    hashnode_t *node;

    init_stats(&stats);

    for(node = wordhash_first(wordhash); node != NULL; node = wordhash_next(wordhash))
    {
	char *token = node->key;
	double prob = compute_probability( token );

	populate_stats( &stats, token, prob, 1 );
    }

    return (&stats);
}

double compute_spamicity(bogostat_t *stats)
// computes the spamicity of the words in the bogostat structure
// returns:  the spamicity
{
    double product, invproduct;
    double spamicity = 0.0;

    discrim_t *pp;

    if (verbose)
    {
	// put the stats in ascending order by probability and alphabet
	qsort(stats->extrema, SIZEOF(stats->extrema), sizeof(discrim_t), compare_stats);
    }

    // Bayes' theorem.
    // For discussion, see <http://www.mathpages.com/home/kmath267.htm>.
    product = invproduct = 1.0f;
    for (pp = stats->extrema; pp < stats->extrema+SIZEOF(stats->extrema); pp++)
	if (pp->prob != 0)
	{
	    product *= pp->prob;
	    invproduct *= (1 - pp->prob);
	    spamicity = product / (product + invproduct);
	    if (verbose>1)
		fprintf(stderr, "#  %f  %f  %s\n", pp->prob, spamicity, pp->key);
	}

    return spamicity;
}

rc_t bogofilter(int fd, double *xss)
/* evaluate text for spamicity */
{
    rc_t	status;
    double 	spamicity;
    wordhash_t  *wordhash;
    bogostat_t	*stats;
    int		wordcount, msgcount;

//  tokenize input text and save words in a wordhash.
    wordhash = collect_words(fd, &msgcount, &wordcount);

    good_list.active = spam_list.active = TRUE;

    db_lock_reader_list(word_lists);

    good_list.msgcount = db_getcount(good_list.dbh);
    spam_list.msgcount = db_getcount(spam_list.dbh);

//  select the best spam/nonspam indicators.
    stats = select_indicators(wordhash);

    db_lock_release_list(word_lists);

//  computes the spamicity of the spam/nonspam indicators.
    spamicity = compute_spamicity(stats);

    status = (spamicity > SPAM_CUTOFF) ? RC_SPAM : RC_NONSPAM;

    if (xss != NULL)
        *xss = spamicity;
    
    if (update)
      register_words((status==RC_SPAM) ? REG_SPAM : REG_GOOD, wordhash, msgcount, wordcount);

    wordhash_free(wordhash);

    return status;
}

// Done
