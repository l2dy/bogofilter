/* $Id$ */
/*
 * $Log$
 * Revision 1.26  2002/10/03 17:57:06  relson
 * Changed interface to lexer.l as a first step towards adding the advanced
 * tokenizing features of spambayes into bogofilter.
 *
 * Thanks to Mark Hoffman for this work.
 *
 * Revision 1.25  2002/10/02 17:19:42  relson
 * Removed unneeded #include statements.
 *
 * Revision 1.24  2002/10/02 17:09:04  gyepi
 * switch to general database locking protocol
 *
 * Revision 1.23  2002/10/02 16:27:40  relson
 * Initial inclusion of multiple wordlist code into bogofilter.
 *
 * Revision 1.22  2002/10/02 16:12:53  relson
 * Added SIZEOF(array) macro for readability of for loops iterating over arrays, particularly the extrema array.
 *
 * Revision 1.21  2002/09/29 03:40:54  gyepi
 *
 * Modified: bogofilter.c bogofilter.h main.c
 * 1. replace Judy with hash table (wordhash)
 * 2. ensure that databases are always locked in the same order.
 *
 * Apologies for simultaneously submitting loosely related changes.
 *
 * Revision 1.20  2002/09/27 01:18:38  gyepi
 * removed unused #defines and logprint function
 *
 * Revision 1.19  2002/09/26 23:04:40  relson
 * documentation:
 *     changed to refer to "good" and "spam" tokens and lists.
 *     removed '-l' option as this function is now in bogoutil.
 *
 * filenames:
 *     changed database from "hamlist.db" to "goodlist.db".
 *
 * variables:
 *     renamed "ham_list" and "hamness" to "good_list" and "goodness".
 *
 * Revision 1.18  2002/09/25 00:51:07  adrian_otto
 * Removed referenced to lock.c and lock.h, because they have been obviated.
 *
 * Revision 1.17  2002/09/24 19:47:49  m-a
 * Add missing #include "datastore.h". Drop unused strlwr.
 *
 * Revision 1.16  2002/09/24 04:34:19  gyepi
 *
 *
 *  Modified Files:
 *  	Makefile.am  -- add entries for datastore* + and other new files
 *         bogofilter.c bogofilter.h main.c -- fixup to use database abstraction
 *
 *  Added Files:
 *  	datastore_db.c datastore_db.h datastore.h -- database abstraction. Also implements locking
 * 	xmalloc.c xmalloc.h -- utility
 *  	bogoutil.c  -- dump/restore utility.
 *
 * 1. Implements database abstraction as discussed.
 *    Also implements multiple readers/single writer file locking.
 *
 * 2. Adds utility to dump/restore databases.
 *
 * Revision 1.15  2002/09/23 11:35:51  m-a
 * Fix GCC 3.2 warnings.
 *
 * Revision 1.14  2002/09/23 11:31:53  m-a
 * Unnest comments, and move $ line down by one to prevent CVS from adding nested comments again.
 *
 * Revision 1.13  2002/09/23 10:08:49  m-a
 * Integrate patch by Zeph Hull and Clint Adams to present spamicity in
 * X-Spam-Status header in bogofilter -p mode.
 *
 * Revision 1.12  2002/09/22 21:26:28  relson
 * Remove the definition and use of strlwr() since get_token() in lexer_l.l already converts the token to lower case.
 *
 * Revision 1.11  2002/09/19 03:20:32  relson
 * Move "msg_prob" assignment to proper function, i.e. from select_indicators() to compute_probability().
 * Move some local variables from the beginning of the function to the innermost block where they're needed.
 *
 * Revision 1.10  2002/09/18 22:41:07  relson
 * Separated probability calculation out of select_indicators() into new function compute_probability().
 *
 * Revision 1.7  2002/09/15 19:22:51  relson
 * Refactor the main bogofilter() function into three smaller, more coherent pieces:
 *
 * void *collect_words(int fd)
 * 	- returns a set of tokens in a Judy array
 *
 * bogostat_t *select_indicators(void  *PArray)
 * 	- processes the set of words
 * 	- returns an array of spamicity indicators (words & probabilities)
 *
 * double compute_spamicity(bogostat_t *stats)
 *    	- processes the array of spamicity indicators
 * 	- returns the spamicity
 *
 * rc_t bogofilter(int fd)
 * 	- calls the 3 component functions
 * 	- returns RC_SPAM or RC_NONSPAM
 *
 * Revision 1.6  2002/09/15 19:07:13  relson
 * Add an enumerated type for return codes of RC_SPAM and RC_NONSPAM, which  values of 0 and 1 as called for by procmail.
 * Use the new codes and type for bogofilter() and when generating the X-Spam-Status message.
 *
 * Revision 1.5  2002/09/15 18:29:04  relson
 * bogofilter.c:
 *
 * Use a Judy array to provide a set of (unique) tokens to speed up the filling of the stat.extrema array.
 *
 * Revision 1.4  2002/09/15 17:41:20  relson
 * The printing of tokens used for computing the spamicity has been changed.  They are now printed in increasing order (by probability and alphabet).  The cumulative spamicity is also printed.
 *
 * The spamicity element of the bogostat_t struct has become a local variable in bogofilter() as it didn't need to be in the struct.
 *
 * Revision 1.3  2002/09/15 16:37:27  relson
 * Implement Eric Seppanen's fix so that bogofilter() properly populates the stats.extrema array.
 * A new word goes into the first empty slot of the array.  If there are no empty slots, it replaces
 * the word with the spamicity index closest to 0.5.
 *
 * Revision 1.2  2002/09/15 16:16:50  relson
 * Clean up underflow checking for word counts by using max() instead of if...then...
 *
 * Revision 1.1.1.1  2002/09/14 22:15:20  adrian_otto
 * 0.7.3 Base Source
 * */
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

#include <bogofilter.h>
#include <datastore.h>
#include <wordhash.h>

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

#define max(x, y)	(((x) > (y)) ? (x) : (y))
#define min(x, y)	(((x) < (y)) ? (x) : (y))

void *collect_words(int fd, int *msg_count, int *word_count)
// tokenize input text and save words in wordhash_t hash table
// returns: the wordhash_t hash table. Sets msg_count and word_count to the appropriate values
{
  int tok = 0;
  int w_count = 0;
  int m_count = 0;
 
  wordprop_t *w;
  hashnode_t *n;
  wordhash_t *h = wordhash_init();
     
  for (;;){
    tok = get_token();
  
    if (tok != FROM && tok != 0){
      w = wordhash_insert(h, yylval, sizeof(wordprop_t));
      w->msg_freq++;
      w_count++;
    }
    else {
      // End of message. Update message counts.
      if (tok == FROM || (tok == 0 && m_count == 0))
        m_count++;
  
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
    *msg_count = m_count;
 
  return(h);
}


void register_words(int fdin, reg_t register_type)
// tokenize text on stdin and register it to  a specified list
// and possibly out of another list
{
  int	wordcount, msgcount;

  hashnode_t *node;
  wordprop_t *wordprop;
  wordhash_t *h;

  wordlist_t *lists[2];
  wordlist_t *incr_list = NULL;
  wordlist_t *decr_list = NULL;
  void *dbh[2];

  int i;
  int nlists = 0;

  h = collect_words(fdin, &msgcount, &wordcount);

  if (verbose)
    fprintf(stderr, "# %d words\n", wordcount);
  
  /* If the operation requires both databases, they must be locked in order */
  switch(register_type)
    {
    case REG_GOOD:
      incr_list = lists[nlists++] = &good_list;
      break;

    case REG_SPAM:
      incr_list = lists[nlists++] = &spam_list;
      break;

    case  REG_GOOD_TO_SPAM:
      decr_list = lists[nlists++] = &good_list;
      incr_list = lists[nlists++] = &spam_list;
      break;

    case REG_SPAM_TO_GOOD:
      incr_list = lists[nlists++] = &good_list;
      decr_list = lists[nlists++] = &spam_list;
      break;
     
    default:
      fprintf(stderr, "Error: Invalid register_type\n");
      exit(2);      
    }

      
  for (i = 0; i < nlists; i++){
    dbh[i] = (void *)lists[i]->dbh;
  }

  db_lock_writer_list(dbh, nlists);

  for (i = 0; i < nlists; i++){
    lists[i]->msgcount = db_getcount(lists[i]->dbh);
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

  for (i = 0; i < nlists; i++){
    db_setcount(lists[i]->dbh, lists[i]->msgcount);
    db_flush(lists[i]->dbh);
    if (verbose)
      fprintf(stderr, "bogofilter: %lu messages on the %s list\n", lists[i]->msgcount, lists[i]->name);
  }

  db_lock_release_list(dbh, nlists);
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

#define SIZEOF(array)	sizeof(array)/sizeof(array[0])

int compare_stats(const void *id1, const void *id2)
{ 
    const discrim_t *d1 = id1;
    const discrim_t *d2 = id2;
    return ( (d1->prob > d2->prob) ||
	     ((d1->prob == d2->prob) && (strcmp(d1->key, d2->key) > 0)));
}

void populate_stats( bogostat_t *stats, char *text, double prob, int count )
// if  the new word,prob pair is a better indicator.
// add them to the stats structure, 
{
    int idx;
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
    int idx;
    for (idx = 0; idx < SIZEOF(stats->extrema); idx++)
    {
	discrim_t *pp = &stats->extrema[idx];
	printf("#  %2d  %f  %s\n", idx, pp->prob, pp->key);
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
    int count;
    int totalcount=0;
    wordprob_t stats;
    double prob;

    wordprob_init(&stats);

    for (list=first_list; list ; list=list->next)
    {
	if (verbose >= 2)
	    printf("checking list %s for word '%s'.\n", list->name, token);
	if (override > list->override) break;
	count=db_getvalue(list->dbh, token);
	if (count) {
	    if (list->ignore)
		return EVEN_ODDS;
	    if (verbose >= 3)
		printf("word '%s' found on list %s with count %d.\n", token, list->name, count);
	    totalcount+=count*list->weight;
	    override=list->override;
	    prob = count;
	    prob /= list->msgcount;
	    prob *= list->weight;
	    if (verbose >= 4)
		printf("word '%s' has uncorrected spamicity %f.\n", token, prob);

	    prob = min(1.0, prob);

	    if (verbose >= 4)
		printf("word '%s' has spamicity %f.\n", token, prob);

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
    discrim_t *pp;
    static bogostat_t stats;
    
    for (pp = stats.extrema; pp < stats.extrema+SIZEOF(stats.extrema); pp++)
    {
 	pp->prob = 0.5f;
 	pp->key[0] = '\0';
    }
    
    for(node = wordhash_first(wordhash); node != NULL; node = wordhash_next(wordhash))
      {
	double prob = compute_probability( node->key );
	double dev = DEVIATION(prob);
	discrim_t *hit = NULL;
	double	hitdev=1;

	// update the list of tokens with maximum deviation
	for (pp = stats.extrema; pp < stats.extrema+SIZEOF(stats.extrema); pp++)
        {
	    double slotdev=DEVIATION(pp->prob);

	    if (dev>slotdev && hitdev>slotdev)
	    {
		hit=pp;
		hitdev=slotdev;
            }
        }
        if (hit) 
	{ 
	    hit->prob = prob;
	    strncpy(hit->key, node->key, MAXTOKENLEN);
	}
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

    if (verbose)
	fprintf(stderr, "#  Spamicity of %f\n", spamicity);

    return spamicity;
}

rc_t bogofilter(int fd, double *xss)
/* evaluate text for spamicity */
{
    rc_t	status;
    double 	spamicity;
    wordhash_t  *wordhash;
    bogostat_t	*stats;
    void *dbh[2];

//  tokenize input text and save words in a wordhash.
    wordhash = collect_words(fd, NULL, NULL);

    //FIXME: This needs to be done as part of the application initialization
    dbh[0] = (void *)good_list.dbh;
    dbh[1] = (void *)spam_list.dbh;

    db_lock_reader_list(dbh, 2);

    good_list.msgcount = db_getcount(good_list.dbh);
    spam_list.msgcount = db_getcount(spam_list.dbh);

//  select the best spam/nonspam indicators.
    stats = select_indicators(wordhash);

    db_lock_release_list(dbh, 2);

//  computes the spamicity of the spam/nonspam indicators.
    spamicity = compute_spamicity(stats);

    status = (spamicity > SPAM_CUTOFF) ? RC_SPAM : RC_NONSPAM;

    if (xss != NULL)
        *xss = spamicity;
    
    wordhash_free(wordhash);

    return status;
}

// Done
