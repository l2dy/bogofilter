/* $Id$ */

/*****************************************************************************

NAME:
   degen.c -- Score a token accorking to the Paul Graham's "degeneration" rules

   compile with: cc -g -o degen -DDEBUG degen.c

AUTHOR:
   David Relson <relson@osagesoftware.com>

REFERENCE:  
  "Better Bayesian Filtering" Jan 2003, http://www.paulgraham.com/better.html

******************************************************************************/

/*
  degen_enabled      -Pd and -PD   (default: -PD - disabled)

  first_match        -Pf and -PF   (default: -PF - disabled)
  which:
      a - evaluate all 17 variations and pick most extreme.
      b - use first of the 17 that matches

  separate_counts    -Ps and -PS  (default: -Ps - enabled)
  storage:
      a - each form separately
      b - one entry for 'word', 'Word', and 'WORD'
          sep - one count  - ham or  spam
	  com - two counts - ham and spam
 */

/* *** From:  "Better Bayesian Filtering" ***
**
** Here are the alternatives [7] considered if the filter sees ``FREE!!!''
** in the Subject line and doesn't have a probability for it.
**
** Subject*FREE!!! Subject*Free!!! Subject*free!!!
** Subject*FREE!   Subject*Free!   Subject*free!
** Subject*FREE    Subject*Free    Subject*free
** FREE!!!         Free!!!         free!!!
** FREE!           Free!           free!
** FREE            Free            free
** 
** *** Notes:  by DR ***
** 
** 1 - Exact match
** 2 - All upper case to 1 leading cap
** 3 - 1 leading cap (or mixed case) to all lower case
**
** 4,5,6 - Multiple trailing exclamation points to 1 (then 1, 2, 3)
**
** 7,8,9 - Remove trailing exclamation point (then 1, 2, 3)
**
** 10-18 - Remove leading context and try (then 1...9)
**
** Use value farthest from 0.5 for unmatched token.
**
** Footnote 7:
**
** Degenerating from uppercase to any-case not feasible as there's no
** easy way to query BerkeleyDB to find that "FREE" is matched by "FrEe"
**
** "Anywhere*foo" is an additional token that is updated when any form of
** "foo" is encountered.  It seems that this would save the multiple
** lookups (described above) and would significantly increase wordlist
** size.
**/

#include <ctype.h>
#include <math.h>
#include <stdlib.h>

#ifndef	DEBUG
#include <config.h>
#include "common.h"

#include "degen.h"
#include "xmalloc.h"
#include "robinson.h"
#else
#include <stdio.h>
#define	xmalloc	malloc
#define	xfree	free
#endif

#define	TAG	  ':'	/* tag delimiter */

#ifndef	DEBUG
#define	TERM	  '!'	/* terminal exclamation point */
#else
#define	TERM	  '+'	/* (debugging substitute) */
#endif

extern double robx;

#define	HAVE_SCORE_1(f)	(fabs(f-EVEN_ODDS) - fabs(ROBX - EVEN_ODDS) > EPS)
#define	HAVE_SCORE_2(f)	(fabs(f-ROBX) > EPS)
#define	HAVE_SCORE_3(f)	(fabs(f-EVEN_ODDS) > EPS)
#define	HAVE_SCORE(f)	HAVE_SCORE_1(f)

/* Function Prototypes */

double get_score(const word_t *token, wordprop_t *wordstats);
double lookup(const word_t *token, wordprop_t *wordstats, double old);

/* Function Definitions */

double get_score(const word_t *token, wordprop_t *wordstats)
{
    double score;
#ifdef	DEBUG
    score = 0.0;
    printf( "%s\n", token );
#else
    wordstats->good = wordstats->bad = 0;
    score = lookup_and_score(token, wordstats);
#endif
    return score;
}

double lookup(const word_t *token, wordprop_t *wordstats, double old)
{
    double new = get_score(token, wordstats);
#if	0
    double ans = (fabs(new-EVEN_ODDS) > fabs(old-EVEN_ODDS)) ? new : old;
#else
    double ans;
    if (fabs(new-EVEN_ODDS) > fabs(old-EVEN_ODDS)) 
	ans = new;
    else
	ans = old;
#endif
    if (DEBUG_SPAMICITY(2)) {
	fputs("***  ", dbgout);
	word_puts(token, 0, dbgout);
	fprintf(dbgout, " - o: %f, n: %f, a: %f\n", old, new, ans );
    }

    return ans;
}

double degen(const word_t *token, wordprop_t *wordstats)
{
    bool first = true;
    int len = token->leng;
    const unsigned char *text = token->text;

    double score = EVEN_ODDS;
    int i, t;
    int excl = 0, alpha = 0, upper = 0, lower = 0, tag = 0;

    for (i = 0; i < len; i += 1) {
	unsigned char c = text[i];

	/* if tag delimiter, record it and clear char counts */
	if (c == TAG) {
	    tag = i+1;
	    alpha = upper = lower = 0;
	}

	/* am interested in upper and lower case letters */
	if (isupper(c)) {		/* count upper case */
	    alpha += 1;
	    upper += 1;
	}

	if (islower(c)) {		/* count lower case */
	    alpha += 1;
	    lower += 1;
	}
    }

    /* count trailing exclamation points */
    for (i = len-1; i > 0 && text[i] == TERM; i -= 1)
	excl += 1;
    
    /* loop for original and original w/o 'tag:' */
    for (t = 0; ; t += tag, tag = 0) {
	int l = len - t;
	int x = excl;
	word_t *copy = word_dup(token);

	/* loop for terminal exclamation points */
	while (x >= 0) {
	    memcpy(copy->text, text + t, l);
	    copy->text[l] = '\0';
	    copy->leng = l;
	    if (!first)
		score = lookup(copy, wordstats, score);	/* score original */
	    first = false;
	    if (first_match && HAVE_SCORE(score))
		return score;
	    if (upper != 0) {				/* if any capital letters */
		for (i=tag+1; i<l; i+=1) {
		    copy->text[i] = tolower(copy->text[i]);
		}
		if (isupper(copy->text[tag])) {		/* score 'Cap.low' */
		    score = lookup(copy, wordstats, score);
		    if (first_match && HAVE_SCORE(score))
			return score;
		    copy->text[tag] = tolower(copy->text[tag]);
		}
		score = lookup(copy, wordstats, score);	/* score 'all.low' */
		if (first_match && HAVE_SCORE(score))
		    return score;
	    }
    
	    if (x <= 1) {	/* If just one, remove it */
		l -= 1;
		x -= 1;
	    } 
	    else {		/* If more than one, reduce to one */
		l = l - (x - 1);
		x = 1;
	    }
	}
	word_free(copy);
	if (tag == 0 || t > 0)
	    break;
    }
    return score;
}

#ifdef	DEBUG
void usage(void)
{
    fprintf(stderr, "Usage: degen token\n");
    fprintf(stderr, "where 'token' may have leading tag, e.g. 'Subj:', and\n");
    fprintf(stderr, "      trailing '+' (plus signs) instead of '!' (exclamation points)\n");
    fprintf(stderr, "      (for bash compatibility)\n");
}
#endif

#ifdef	DEBUG
int main(int argc, char **argv)
{
    int i;

    if (argc < 2)
	usage();

    for (i = 1; i < argc; i += 1) {
	const char *token = argv[i];
	size_t len = strlen(token);
	degen(token, len);
    }

    exit(EXIT_SUCCESS);
}
#endif
