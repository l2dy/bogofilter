/* $Id$ */

/*****************************************************************************

NAME:
   collect.h -- global definitions for the collect library, part of bogofilter

******************************************************************************/

#ifndef COLLECT_H
#define COLLECT_H

#include "token.h"
#include "wordhash.h"

extern void	wordprop_init(void *vwordprop);
extern void	wordprop_incr(wordprop_t *w1, wordprop_t *w2);
extern void	collect_words(wordhash_t *wh);

#endif
