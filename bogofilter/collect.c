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

int	max_repeats;

/* this is referenced by register.c, must not be static */
void wordprop_init(void *vwordprop){
	wordprop_t *wordprop = vwordprop;
	wordprop->freq = 0;
}

static bool from_seen = false;

void collect_reset(void)
{
    from_seen = false;
}

/* Tokenize input text and save words in the allocated wordhash_t hash table.
 * The caller must free the returned wordhash.
 *
 * Sets *word_count to the count of total tokens seen if word_count 
 * is non-NULL.
 *
 * Stores the freshly-allocated wordhash into *wh. wh must not be NULL.
 *
 * *cont is set to true if the EOF token has not been read. cont must
 * not be NULL.
 */
void collect_words(/*@out@*/ wordhash_t **wh,
       /*@out@*/ /*@null@*/ long *word_count, /*@out@*/ bool *cont)
{
    long w_count = 0;

    wordprop_t *w;
    wordhash_t *h = wordhash_init();

    for (;;){
	token_t token_type = get_token();

	if (token_type != FROM && token_type != 0){
	    w = wordhash_insert(h, yylval, sizeof(wordprop_t), &wordprop_init);
	    if (w->freq < max_repeats) w->freq++;
	    w_count++;
	    continue;
	}

	if (token_type == FROM && from_seen == false) {
	    from_seen = true;
	    continue;
	}

	/* Want to process EOF, *then* drop out */
	*cont = (token_type != 0);
	break;
    }

    if (word_count)
	*word_count = w_count;

    *wh = h;
}
