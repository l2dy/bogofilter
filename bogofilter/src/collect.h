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
extern void	collect_reset(void);
extern token_t	collect_words(wordhash_t *wh);

#endif
