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
void wordprop_init(void *vwordprop)
{
    wordprop_t *wordprop = vwordprop;
    wordprop->freq = 0;
    wordprop->good = 0;
    wordprop->bad  = 0;
    wordprop->prob = 0;
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
token_t collect_words(wordhash_t *wh)
{
    token_t class;

    wordprop_t *w;

    if (DEBUG_WORDLIST(2)) fprintf(dbgout, "### collect_words() begins\n");

    for (;;){
	token_t token_type;
	class = get_token();

	/* treat .MSG_COUNT as FROM */
	token_type = (class != MSG_COUNT_LINE) ? class : FROM;

	if (token_type != FROM && 
	    token_type != NONE)
	{
	    word_t *token = yylval;

	    if (token_type == BOGO_LEX_LINE)
	    {
		char *s = yylval->text+1;
		char *f = strchr(s, '"');
		token = word_new(s, f-s);
	    }

	    w = wordhash_insert(wh, token, sizeof(wordprop_t), &wordprop_init);
	    if (w->freq < max_repeats) w->freq++;
	    wh->wordcount += 1;
	    if (DEBUG_WORDLIST(3)) { 
		fprintf(dbgout, "%3d ", (int) wh->wordcount);
		word_puts(token, 0, dbgout);
		fputc('\n', dbgout);
	    }

	    if (token_type == BOGO_LEX_LINE)
	    {
		char *s = yylval->text+1;

		s = strchr(s, '"');
		w->bad = atoi(s+2);
		s = strchr(s+2, ' ');
		w->good = atoi(s+1);

		word_free(token);
	    }
	    continue;
	}

	if (token_type == FROM && from_seen == false) {
	    from_seen = true;
	    continue;
	}
	break;
    }

    if (DEBUG_WORDLIST(2)) fprintf(dbgout, "### collect_words() ends\n");

    return class;
}
