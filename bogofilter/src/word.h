/* $Id$ */

/*****************************************************************************

NAME:
   word.h -- constants and declarations for word.c

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#ifndef	WORD_H
#define	WORD_H

#define	D	1	/* size adjustment */
#define	Z(n)	n='\0'	/* mark end of string */

typedef struct {
    size_t leng;
    byte   *text;
} word_t;

extern word_t  *word_new(const byte *text, size_t leng);
extern void 	word_free(word_t *self);
extern word_t  *word_dup(const word_t *self);
extern word_t  *word_cpy(word_t *dst, const word_t *src);
extern int 	word_cmp(const word_t *w1, const word_t *w2);
extern void 	word_puts(const word_t *self, FILE *fp);

#endif	/* WORD_H */

