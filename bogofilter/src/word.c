/* $Id$ */

/*****************************************************************************

NAME:
   word.c -- support for bogofilter's word struct

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#include "config.h"

#include "common.h"
#include "word.h"
#include "xmalloc.h"

/* Function Definitions */

word_t *word_new(const byte *text, size_t leng)
{
    /* to lessen malloc/free calls, allocate struct and data in one block */
    word_t *self = xmalloc(sizeof(word_t)+leng+D);
    self->leng = leng;
    self->text = (byte *)((char *)self+sizeof(word_t));
    if (text) {
	memcpy(self->text, text, leng);
	Z(self->text[self->leng]);		/* debug code - remove */
    }
    return self;
}

void word_free(word_t *self)
{
    xfree(self);
}

word_t *word_dup(const word_t *word)
{
    word_t *self = xmalloc(sizeof(word_t));
    self->leng = word->leng;
    self->text = xmalloc(word->leng+D);
    if (word->text) {
	memcpy(self->text, word->text, self->leng);
	Z(self->text[self->leng]);		/* debug code - remove */
    }
    return self;
}

word_t *word_cpy(word_t *dst, const word_t *src)
{
    dst->leng = src->leng;
    memcpy(dst->text, src->text, src->leng+D);
    Z(dst->text[dst->leng]);			/* debug code - remove */
    return dst;
}

int word_cmp(const word_t *w1, const word_t *w2)
{
#if	1
    return strcmp((const char *)w1->text, (const char *)w2->text);
#else
    size_t s1 = w1->leng;
    size_t s2 = w2->leng;
    size_t l  = min(s1, s2);
    size_t i;

    for (i = 0; i < l ; i += 1) {
	int d = w1->text[i] -  w2->text[i];
	if (d != 0)
	    return d;
    }

    return s1 - s2;
#endif
}

void word_puts(const word_t *self, size_t width, FILE *fp)
{
    /* width = 0 - output all of the word
    **       > 0 - use 'width' as count, 
    **		   blank fill if 'width' < length
    */
    size_t l = (width == 0) ? self->leng : min(width, self->leng);
    fwrite(self->text, 1, l, fp);
    if (l < width)
	(void) fprintf(fp, "%*s", width - l," ");
}
