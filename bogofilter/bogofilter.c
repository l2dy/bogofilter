/* $Id$ */
/*
 * $Log$
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
#include "lock.h"

// implementation details
#define HEADER		"# bogofilter email-count (format version B): %lu\n"

// constants for the Graham formula 
#define HAM_BIAS	2		// give ham words more weight
#define KEEPERS		15		// how many extrema to keep
#define MINIMUM_FREQ	5		// minimum freq
#define UNKNOWN_WORD	0.4f		// odds that unknown word is spammish
#define SPAM_CUTOFF	0.9f		// if it's spammier than this...
#define MAX_REPEATS	4		// cap on word frequency per message

#define DEVIATION(n)	fabs((n) - 0.5f)	// deviation from average

#define max(x, y)	(((x) > (y)) ? (x) : (y))
#define min(x, y)	(((x) < (y)) ? (x) : (y))

wordlist_t ham_list	= {"ham", NULL, 0, NULL};
wordlist_t spam_list	= {"spam", NULL, 0, NULL};

#define	PLURAL(count) ((count == 1) ? "" : "s")

#define DBT_init(dbt) do { memset(&dbt, 0, sizeof(DBT)); } while(0)
#define char2DBT(dbt,ptr) do { dbt.data = ptr; dbt.size = strlen(ptr); } while(0)

#define x2DBT(dbt,val,type)  do { dbt.data = &val; dbt.size = sizeof(type); } while(0)

#define long2DBT(dbt,val) x2DBT(dbt,val,long)  
#define int2DBT(dbt,val)  x2DBT(dbt,val,int)

long get_word_value(char *word, wordlist_t *list)
{
    DB *dbp;
    DBT key;
    DBT data;
    int ret;

    DBT_init(key);
    DBT_init(data);

    char2DBT(key, word);

    dbp = list->db;
	
    if ((ret = dbp->get(dbp, NULL, &key, &data, 0)) == 0){
	return(*(long *)data.data);
    }
    else if (ret == DB_NOTFOUND){
	return(0);
    }
    else {
	dbp->err (dbp, ret, "bogofilter (get_word_value): %s", word);
	exit(2);
    }
}

void set_word_value(char *word, long value, wordlist_t *list)
{
    DB *dbp;
    DBT key;
    DBT data;
    int ret;

    DBT_init(key);
    DBT_init(data);

    char2DBT(key, word);
    long2DBT(data, value);
        
    dbp = list->db;

    if ((ret = dbp->put(dbp, NULL, &key, &data,0)) == 0){
	if (verbose >= 3)
            (void) printf("\"%s\" stored %ld time%s\n", word, value, PLURAL(value));
    }
    else 
    {
	dbp->err (dbp, ret, "bogofilter (set_word_value): %s", word);
	exit(2);
    }
}

static void increment(char *word,  long incr, wordlist_t *list)
/* increment a word usage count in the specified list */
{
  long count = get_word_value(word, list) + incr;
  count = max(count, 0);
 
  set_word_value(word, count, list);

  if (verbose >= 1) {
    printf("increment: '%s' has %lu hits\n",word,count);
  }
}

static int getcount(char *word, wordlist_t *list)
/* get the count associated with a given word in a list */
{
  long value = get_word_value(word, list);

  if (value){
    if (verbose >= 2)
      printf("getcount: '%s' has %ld %s hits in %ld\n", word, value, list->name, list->msgcount);
  }
  else {
      if (verbose >= 3)
	  printf("getcount: no %s hits for %s\n", list->name, word);
  }

  return value; 
}

int read_count(wordlist_t *list)
/* Reads count of emails, if any. */ 
{
    FILE	*infp;

    list->msgcount = 0;

    infp = fopen(list->count_file, "r");   /* Open file for reading */

    if (infp == NULL)
	return 1;

    lock_fileno(fileno(infp), LOCK_SH);    /* Lock the fole before reading */
    fscanf(infp, HEADER, &list->msgcount); /* Read contents from the file */
    unlock_fileno(fileno(infp));           /* Release the lock */
    fclose(infp);
    return 0;
}


void write_count(wordlist_t *list)
/* dump the count of emails to a specified file */
{
    FILE	*outfp;

    outfp = fopen(list->count_file, "a+"); /* First open for append */

    if (outfp == NULL)
    {
	fprintf(stderr, "bogofilter (write_count): cannot open file %s. %m", 
		list->count_file);
	exit(2);
    }

    /* Lock file before modifying it to avoid a race condition with other
     * bogofilter instances that may want to read/modify this file */
    lock_fileno(fileno(outfp), LOCK_EX);   /* Lock the file for writing */
    freopen(list->count_file, "w", outfp); /* Empty the file, ready to write */
    (void) fprintf(outfp, HEADER, list->msgcount);
    unlock_fileno(fileno(outfp));          /* Unlock the file */
    fclose(outfp);

}


int read_list(wordlist_t *list)
/* initialize database */
/* return 0 if successful, and 1 if it was unsuccessful. */
{
    int ret;
    int fdp; /* for holding the value of the db file descriptor */
    DB *dbp;
    list->file = strdup(list->file);

    dbp = malloc(sizeof(DB));
    
    if (dbp == NULL){
	fprintf(stderr, "bogofilter (readlist): out of memory\n");
	return 1;
    }
    
    if ((ret = db_create (&dbp, NULL, 0)) != 0){
	   fprintf (stderr, "bogofilter (db_create): %s\n", db_strerror (ret));
	   return 1;
    }

    /* Lock the database file */
    if(dbp->fd(dbp, &fdp) == 0) {           /* Get file descriptor to lock */
    	if(lock_fileno(fdp,LOCK_SH) != 0) { /* Get a shared lock */
		return(1);                  /* Lock attempt failed */
	}
    }

    if ((ret = dbp->open (dbp, list->file, NULL, DB_BTREE, DB_CREATE, 0664)) != 0){
           dbp->err (dbp, ret, "open: %s", list->file);
	   return 1;
    }

    list->db = dbp;
    read_count(list);
    
    return 0;
}

void write_list(wordlist_t *list)
/* close database */
{
    int fdp; /* for holding the value of the db file descriptor */
    DB *db = list->db;

    write_count(list);

    /* Unock the database file */
    if(db->fd(db, &fdp) == 0) { /* Get file descriptor to unlock */
    	unlock_fileno(fdp);     /* Release lock */
    }

    db->close(db, 0);
}

int bogodump(char *file)
/* dump state of database */
{
    int ret;
    DB db;
    DB *dbp;
    DBC dbc;
    DBC *dbcp;
    DBT key, data;
  
    dbp = &db;
    dbcp = &dbc;

    if ((ret = db_create (&dbp, NULL, 0)) != 0)
    {
	fprintf (stderr, "bogodump (db_create): %s\n", db_strerror (ret));
	return 1;
    }

    if ((ret = dbp->open (dbp, file, NULL, DB_BTREE, 0, 0)) != 0)
    {
	dbp->err (dbp, ret, "bogodump (open): %s", file);
	return 1;
    }

    if ((ret = dbp->cursor (dbp, NULL, &dbcp, 0) != 0))
    {
	dbp->err (dbp, ret, "bogodump (cursor): %s", file);
	return 1;
    }

    memset (&key, 0, sizeof (DBT));
    memset (&data, 0, sizeof (DBT));

    for (;;)
    {
	ret = dbcp->c_get (dbcp, &key, &data, DB_NEXT);
	if (ret == 0){
	    printf ("%.*s:%lu\n",key.size, (char *)key.data, *(unsigned long *)data.data);
	}
	else if (ret == DB_NOTFOUND){
	    break;
	}
	else {
	    dbp->err (dbp, ret, "bogodump (c_get)");
	    break;
	}
    }
    return 0;
}

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

    // Grab tokens from the lexical analyzer into our own private Judy array
    yyin = fdopen(fdin, "r");
    wordcount = msgcount = 0;
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

		increment(tokenbuffer, freq, list);
		if (other)
		    increment(tokenbuffer, -freq, other);
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

int compare_stats(discrim_t *d1, discrim_t *d2)
{ 
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
    double prob, hamness, spamness;

    hamness = getcount(token, &ham_list);
    spamness = getcount(token, &spam_list);

#ifdef NON_EQUIPROBABLE
    // There is an argument that we should by by number of *words* here.
    double	msg_prob = (spam_list.msgcount / ham_list.msgcount);
#endif // NON_EQUIPROBABLE

    // Paul Graham's original formula:
    // 
    // (let ((g (* 2 (or (gethash word ham) 0))) 
    //      (b (or (gethash word spam) 0)))
    //  (unless (&lt; (+ g b) 5) 
    //   (max .01 (min .99 
    //  	    (double (/ 
    // 		    (min 1 (/ b nspam)) 
    // 		    (+ (min 1 (/ g nham)) (min 1 (/ b nspam)))))))))
    // This assumes that spam and non-spam are equiprobable.
    hamness *= HAM_BIAS;
    if (hamness + spamness < MINIMUM_FREQ)
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
	register double pg = min(1, (hamness / ham_list.msgcount));

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
		printf("#  %f  %f  %s\n", pp->prob, spamicity, pp->key);
	}

    if (verbose)
	printf("#  Spamicity of %f\n", spamicity);

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
    
//  select the best spam/nonspam indicators.
    stats = select_indicators(PArray);
    
//  computes the spamicity of the spam/nonspam indicators.
    spamicity = compute_spamicity(stats);

    status = (spamicity > SPAM_CUTOFF) ? RC_SPAM : RC_NONSPAM;

    if (xss != NULL)
        *xss = spamicity;

    return status;
}

// Done
