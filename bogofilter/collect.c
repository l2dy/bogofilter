#include <stdio.h>
#include <stdlib.h>

#include <config.h>

#include "common.h"
#include "bogofilter.h"
#include "register.h"
#include "collect.h"
#include "wordhash.h"


#include "lexer.h"

void wordprop_init(void *vwordprop){
	wordprop_t *wordprop = vwordprop;

	wordprop->freq = 0;
}

static bool from_seen = false;

void collect_reset(void)
{
    from_seen = false;
}

void collect_words(/*@out@*/ wordhash_t **wh,
       /*@out@*/ /*@null@*/ long *word_count, /*@out@*/ bool *cont)
    /* tokenize input text and save words in wordhash_t hash table 
     * Sets word_count to the appropriate values
     * if the pointer is non-NULL.
     * wh and cont must not be NULL
     * cont is set if further data is available.
     */
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
	} else {
	    if (token_type == FROM && from_seen == false) {
		from_seen = true;
		continue;
	    }

	    /* Want to process EOF, *then* drop out */
	    *cont = (token_type != 0);
	    break;
	}
    }

    if (word_count)
	*word_count = w_count;

    *wh = h;
}
