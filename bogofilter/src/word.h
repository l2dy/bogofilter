/* $Id$ */

/*****************************************************************************

NAME:
   word.h -- constants and declarations for word.c

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#ifndef	WORD_H
#define	WORD_H

#include "bftypes.h"

typedef struct {
    uint leng;
    byte *text;
} word_t;

extern word_t  *word_new(const byte *text, uint leng);
extern void 	word_free(word_t *self);
extern word_t  *word_dup(const word_t *self);
extern word_t  *word_cpy(word_t *dst, const word_t *src);
extern int 	word_cmp(const word_t *w1, const word_t *w2);
extern word_t  *word_concat(const word_t *w1, const word_t *w2);
extern void 	word_puts(const word_t *self, uint width, FILE *fp);

/** Compare word \a w to string \a s. */
extern int	word_cmps(const word_t *w, const char *s);

#endif	/* WORD_H */
