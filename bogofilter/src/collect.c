/* $Id$ */

/* collect.c -- tokenize input and cap word frequencies, return a wordhash */

#include <stdio.h>
#include <stdlib.h>

#include <config.h>
#include "common.h"

#include "bogofilter.h"
#include "wordhash.h"
#include "token.h"

#include "collect.h"

/* this is referenced by register.c, must not be static */
void wordprop_init(void *vwordprop){
	wordprop_t *wordprop = vwordprop;
	wordprop->freq = 0;
}

static bool from_seen = false;

#if	0	/* 01/26/2003 - not used */
void collect_reset(void)
{
    from_seen = false;
}
#endif

/* Tokenize input text and save words in the wordhash_t hash table.
 *
 * Returns:  true if the EOF token has not been read.
 */
bool collect_words(wordhash_t *wh)
{
    bool more;

    wordprop_t *w;

    if (DEBUG_WORDLIST(2)) fprintf(dbgout, "### collect_words() begins\n");

    for (;;){
	token_t token_type = get_token();

	if (token_type != FROM && token_type != NONE){
	    w = wordhash_insert(wh, yylval, sizeof(wordprop_t), &wordprop_init);
	    if (w->freq < max_repeats) w->freq++;
	    wh->wordcount += 1;
	    if (DEBUG_WORDLIST(3)) { 
		fprintf(dbgout, "%3d ", (int) wh->wordcount);
		word_puts(yylval, 0, dbgout);
		fputc('\n', dbgout);
	    }
	    continue;
	}

	if (token_type == FROM && from_seen == false) {
	    from_seen = true;
	    continue;
	}

	/* Process more input if FROM, but not if EOF */
	more = token_type != NONE;
	break;
    }

    if (DEBUG_WORDLIST(2)) fprintf(dbgout, "### collect_words() ends\n");

    return more;
}
