/* collect.h -- global definitions for the collect library, part of bogofilter */
/* $Id$ */

#ifndef COLLECT_H
#define COLLECT_H

#include "wordhash.h"

extern void wordprop_init(void *vwordprop);

extern bool collect_words(wordhash_t *wh);

#if	0	/* 01/26/2003 - not used */
void collect_reset(void);
#endif

#endif
