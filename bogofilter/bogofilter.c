/* $Id$ */
/*
 * $Log$
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
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <db.h>
#include <Judy.h>
#include "bogofilter.h"
#include "datastore.h"

// implementation details
#define HEADER		"# bogofilter email-count (format version B): %lu\n"

// constants for the Graham formula 
#define GOOD_BIAS	2		// give good words more weight
#define KEEPERS		15		// how many extrema to keep
#define MINIMUM_FREQ	5		// minimum freq
#define UNKNOWN_WORD	0.4f		// odds that unknown word is spammish
#define SPAM_CUTOFF	0.9f		// if it's spammier than this...
#define MAX_REPEATS	4		// cap on word frequency per message

#define DEVIATION(n)	fabs((n) - 0.5f)	// deviation from average

#define max(x, y)	(((x) > (y)) ? (x) : (y))
#define min(x, y)	(((x) < (y)) ? (x) : (y))

wordlist_t good_list	= {"good", NULL, 0, NULL};
wordlist_t spam_list	= {"spam", NULL, 0, NULL};

#define	PLURAL(count) ((count == 1) ? "" : "s")

void register_words(int fdin, wordlist_t *list, wordlist_t *other)
// tokenize text on stdin and register it to  a specified list
// and possibly out of another list
{
    int	tok, wordcount, msgcount;
    void  **PPValue;			// associated with Index.
    void  *PArray = (Pvoid_t) NULL;	// JudySL array.
    JError_t JError;                    // Judy error structure
    void	**loc;
    char	tokenbuffer[BUFSIZ];

    //FIXME -- The database locking time can be minized by using a hash table.
    db_lock_writer(list->dbh);
    if (other)
      db_lock_writer(other->dbh);

    // Grab tokens from the lexical analyzer into our own private Judy array
    yyin = fdopen(fdin, "r");
    msgcount = wordcount = 0;

    list->msgcount = db_getcount(list->dbh);
    if (other) other->msgcount = db_getcount(other->dbh);

    for (;;)
    {
	tok = get_token();

	if (tok != FROM && tok != 0)
	{
	    // Ordinary word, stash in private per-message array.
	    if ((PPValue = JudySLIns(&PArray, yytext, &JError)) == PPJERR)
		return;
	    (*((PWord_t) PPValue))++;
	    wordcount++;
	}
	else
	{
	    // End of message. Update message counts.
	    if (tok == FROM || (tok == 0 && msgcount == 0))
	    {
		list->msgcount++;
                msgcount++;
		if (other && other->msgcount > 0)
		    other->msgcount--;
	    }

	    // We copy the incoming words into their own per-message array
	    // in order to be able to cap frequencies.
	    tokenbuffer[0]='\0';
	    for (loc  = JudySLFirst(PArray, tokenbuffer, 0);
		 loc != (void *) NULL;
		 loc  = JudySLNext(PArray, tokenbuffer, 0))
	    {
		int freq	= (*((PWord_t) loc));

		if (freq > MAX_REPEATS)
		    freq = MAX_REPEATS;

		db_increment(list->dbh, tokenbuffer, freq);
		if (other)
		    db_increment(other->dbh, tokenbuffer, -freq);
	    }
	    JudySLFreeArray(&PArray, &JError);
	    PArray = (Pvoid_t)NULL;

	    if (verbose)
		printf("# %d words\n", wordcount);

	    // Want to process EOF, *then* drop out
	    if (tok == 0)
		break;
	}
    }

    db_setcount(list->dbh, list->msgcount);
    db_flush(list->dbh);
    if (verbose)
       fprintf(stderr, "bogofilter: %lu messages on the %s list\n", list->msgcount, list->name);

    if (other){
	   	 db_setcount(other->dbh, other->msgcount);
		 if (verbose)
		      fprintf(stderr, "bogofilter: %lu messages on the %s list\n", other->msgcount, other->name);

		 db_flush(other->dbh);
    		 db_lock_release(other->dbh);
    }
    
    db_lock_release(list->dbh);
}

#ifdef __UNUSED__
void logprintf(const char *fmt, ... )
// log data from server
{
    char buf[BUFSIZ];
    va_list ap;
    int fd;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    
    fd=open("/tmp/bogolog", O_RDWR|O_CREAT|O_APPEND,0700);
    write(fd,buf,strlen(buf));
    close(fd);
}
#endif // __UNUSED__

typedef struct 
{
    char        key[MAXWORDLEN+1];
    double      prob;
}
discrim_t;

typedef struct
{
    discrim_t extrema[KEEPERS];
}
bogostat_t;

int compare_stats(const void *id1, const void *id2)
{ 
    const discrim_t *d1 = id1;
    const discrim_t *d2 = id2;
    return ( (d1->prob > d2->prob) ||
	     ((d1->prob == d2->prob) && (strcmp(d1->key, d2->key) > 0)));
}

void *collect_words(int fd)
// tokenize input text and save words in a Judy array.
// returns:  the Judy array
{
    int tok;

    void	**PPValue;			// associated with Index.
    void	*PArray = (Pvoid_t) NULL;	// JudySL array.
    JError_t	JError;				// Judy error structure

    yyin = fdopen(fd, "r");
    while ((tok = get_token()) != 0)
    {
	// Ordinary word, stash in private per-message array.
	if ((PPValue = JudySLIns(&PArray, yytext, &JError)) == PPJERR)
	    break;
	(*((PWord_t) PPValue))++;
    }
    return PArray;
}

double compute_probability( char *token )
{
    double prob, goodness, spamness;
    
    goodness = db_getvalue(good_list.dbh, token);
    spamness = db_getvalue(spam_list.dbh, token);

#ifdef NON_EQUIPROBABLE
    // There is an argument that we should by by number of *words* here.
    double	msg_prob = (spam_list.msgcount / good_list.msgcount);
#endif // NON_EQUIPROBABLE

    // Paul Graham's original formula:
    // 
    // (let ((g (* 2 (or (gethash word good) 0))) 
    //      (b (or (gethash word spam) 0)))
    //  (unless (< (+ g b) 5) 
    //   (max .01 (min .99 
    //  	    (double (/ 
    // 		    (min 1 (/ b nspam)) 
    // 		    (+ (min 1 (/ g ngood)) (min 1 (/ b nspam)))))))))
    // This assumes that spam and non-spam are equiprobable.
    goodness *= GOOD_BIAS;
    if (goodness + spamness < MINIMUM_FREQ)
#ifdef NON_EQUIPROBABLE
	// In the absence of evidence, the probability that a new word
	// will be spam is the historical ratio of spam words to
	// nonspam words.
	prob = msg_prob;
#else
	prob = UNKNOWN_WORD;
#endif // NON_EQUIPROBABLE
    else
    {
	register double pb = min(1, (spamness / spam_list.msgcount));
	register double pg = min(1, (goodness / good_list.msgcount));

#ifdef NON_EQUIPROBABLE
	prob = (pb * msg_prob) / ((pg * (1 - msg_prob)) + (pb * msg_prob));
#else
	prob = pb / (pg + pb);
#endif // NON_EQUIPROBABLE
	prob = min(prob, 0.99);
	prob = max(prob, 0.01);
    }
    return prob;
}

bogostat_t *select_indicators(void  *PArray)
// selects the best spam/nonspam indicators and
// populates the stats structure.
{
    void	**loc;
    char	tokenbuffer[BUFSIZ];

    discrim_t *pp;
    static bogostat_t stats;
    
    for (pp = stats.extrema; pp < stats.extrema+sizeof(stats.extrema)/sizeof(*stats.extrema); pp++)
    {
 	pp->prob = 0.5f;
 	pp->key[0] = '\0';
    }
    
    for (loc  = JudySLFirst(PArray, tokenbuffer, 0);
	 loc != (void *) NULL;
	 loc  = JudySLNext(PArray, tokenbuffer, 0))
    {
	char  *token = tokenbuffer;
	double prob = compute_probability( token );
	double dev = DEVIATION(prob);
	discrim_t *hit = NULL;
	double	hitdev=1;

	// update the list of tokens with maximum deviation
	for (pp = stats.extrema; pp < stats.extrema+sizeof(stats.extrema)/sizeof(*stats.extrema); pp++)
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
	    strncpy(hit->key, token, MAXWORDLEN);
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
	qsort(stats->extrema, KEEPERS, sizeof(discrim_t), compare_stats);
    }

    // Bayes' theorem.
    // For discussion, see <http://www.mathpages.com/home/kmath267.htm>.
    product = invproduct = 1.0f;
    for (pp = stats->extrema; pp < stats->extrema+sizeof(stats->extrema)/sizeof(*stats->extrema); pp++)
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
    void	*PArray = (Pvoid_t) NULL;	// JudySL array.
    bogostat_t	*stats;

//  tokenize input text and save words in a Judy array.
    PArray = collect_words(fd);

    db_lock_reader(good_list.dbh);
    db_lock_reader(spam_list.dbh);

    good_list.msgcount = db_getcount(good_list.dbh);
    spam_list.msgcount = db_getcount(spam_list.dbh);

//  select the best spam/nonspam indicators.
    stats = select_indicators(PArray);
    
//  computes the spamicity of the spam/nonspam indicators.
    spamicity = compute_spamicity(stats);

    db_lock_release(spam_list.dbh);
    db_lock_release(good_list.dbh);

    status = (spamicity > SPAM_CUTOFF) ? RC_SPAM : RC_NONSPAM;

    if (xss != NULL)
        *xss = spamicity;

    return status;
}

// Done
