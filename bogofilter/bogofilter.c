/* $Id$ */
/* $Log$
 * Revision 1.1  2002/09/14 22:15:20  adrian_otto
 * Initial revision
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

static void strlwr(char* s)
{
    char c;
    while((c = *s) != 0)
	*s++ = tolower(c);
}

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
  long count = get_word_value(word, list);
  if ( incr > 0 || count > -incr ) {
      count += incr;
  }
  else {
      count = 0;
  }
 
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
	    strlwr(yytext);
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
    double spamicity;
    discrim_t extrema[KEEPERS];
}
bogostat_t;

int bogofilter(int fd)
/* evaluate text for spamicity */
{
    double prob, product, invproduct;
    double hamness, spamness, slotdev, hitdev;    
    int tok;
#ifdef NON_EQUIPROBABLE
    // There is an argument that we should by by number of *words* here.
    double	msg_prob = (spam_list.msgcount / ham_list.msgcount);
#endif // NON_EQUIPROBABLE

    static bogostat_t stats;
    discrim_t *pp, *hit;

    for (pp = stats.extrema; pp < stats.extrema+sizeof(stats.extrema)/sizeof(*stats.extrema); pp++)
    {
	pp->prob = 0.5f;
	pp->key[0] = '\0';
    }

    yyin = fdopen(fd, "r");
    while ((tok = get_token()) != 0)
    {
	double dev;

	strlwr(yytext);
	hamness = getcount(yytext, &ham_list);
	spamness  = getcount(yytext, &spam_list);

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

	// update the list of tokens with maximum deviation
	dev = DEVIATION(prob);
        hit = NULL;
        hitdev=0;
	for (pp = stats.extrema; pp < stats.extrema+sizeof(stats.extrema)/sizeof(*stats.extrema); pp++)
        {
	    // don't allow duplicate tokens in the stats.extrema
	    if (pp->key && strcmp(pp->key, yytext)==0)
            {
                hit=NULL;
		break;
            }
	    else 
	    {
                slotdev=DEVIATION(pp->prob);
                if (dev>slotdev && dev>hitdev)
                {
                    hit=pp;
		    hitdev=slotdev;
                }
            }
        }
        if (hit) 
	{ 
	    hit->prob = prob;
	    strncpy(hit->key, yytext, MAXWORDLEN);
	}
    }

    if (verbose)
    {
	for (pp = stats.extrema; pp < stats.extrema + KEEPERS; pp++)
	    if (pp->key)
		printf("%s -> %f\n", pp->key, pp->prob);
    }

    // Bayes' theorem.
    // For discussion, see <http://www.mathpages.com/home/kmath267.htm>.
    product = invproduct = 1.0f;
    for (pp = stats.extrema; pp < stats.extrema+sizeof(stats.extrema)/sizeof(*stats.extrema); pp++)
	if (pp->prob == 0)
	    break;
    	else
	{
	    product *= pp->prob;
	    invproduct *= (1 - pp->prob);
	}
    stats.spamicity = product / (product + invproduct);

    if (verbose)
	printf("Spamicity of %f\n", stats.spamicity);

    return((stats.spamicity > SPAM_CUTOFF) ? 0 : 1);
}

// Done

