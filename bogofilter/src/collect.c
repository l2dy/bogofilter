/* $Id$ */

/* collect.c -- tokenize input and cap word frequencies, return a wordhash */

#include "common.h"

#include <stdlib.h>

#include "bogofilter.h"
#include "wordhash.h"
#include "token.h"

#include "collect.h"

/* this is referenced by register.c, must not be static */
void wordprop_init(void *vwordprop)
{
    wordprop_t *wordprop = vwordprop;
    memset(wordprop, 0, sizeof(*wordprop));
}

/* Tokenize input text and save words in the wordhash_t hash table.
 *
 * Returns:  true if the EOF token has not been read.
 */
void collect_words(wordhash_t *wh)
{
    if (DEBUG_WORDLIST(2)) fprintf(dbgout, "### collect_words() begins\n");

    for (;;){
	wordprop_t *w;
	word_t *token;
	token_t cls = get_token();

	if (cls == NONE)
	    break;

	token = yylval;

	if (cls == BOGO_LEX_LINE)
	{
	    char *s = (char *)(yylval->text+1);
	    char *f = strchr(s, '"');
	    token->text = (unsigned char *) s;
	    token->leng = f - s;
	}

	w = wordhash_insert(wh, token, sizeof(wordprop_t), &wordprop_init);
	if (w->freq < max_repeats) w->freq++;
	wh->wordcount += 1;
	if (DEBUG_WORDLIST(3)) { 
	    fprintf(dbgout, "%3d ", (int) wh->wordcount);
	    word_puts(token, 0, dbgout);
	    fputc('\n', dbgout);
	}

	if (cls == BOGO_LEX_LINE)
	{
	    char *s = (char *)yylval->text;
	    s += yylval->leng + 2;
	    w->bad = atoi(s);
	    s = strchr(s+1, ' ') + 1;
	    w->good = atoi(s);
	}
    }

    if (DEBUG_WORDLIST(2)) fprintf(dbgout, "### collect_words() ends\n");

    return;
}
