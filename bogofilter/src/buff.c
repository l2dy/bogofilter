/* $Id$ */

/*****************************************************************************

NAME:
   buff.c -- support for bogofilter's buff struct

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#include <assert.h>

#include "config.h"
#include "common.h"

#include "buff.h"
#include "fgetsl.h"
#include "xmalloc.h"

/* Function Definitions */

buff_t *buff_new(byte *buff, size_t used, size_t size)
{
    buff_t *self = xmalloc(sizeof(buff_t));
    self->t.text = buff;
    self->t.leng = used;
    self->pos  = 0;
    self->read = 0;
    self->size = size;
    return self;
}

void buff_free(buff_t *self)
{
    xfree(self);
}

buff_t *buff_dup(const buff_t *word)
{
    buff_t *self = xmalloc(sizeof(buff_t));
    self->size = word->size;
    self->t.text = word->t.text;
    return self;
}

int buff_fgetsl(buff_t *b, FILE *in)
{
    int s = xfgetsl(b->t.text, b->size, in, 1);
    if (s >= 0) {
	b->t.leng = s;
    }
    b->read = 0;
    return s;
}

void buff_puts(const buff_t *buff, size_t width, FILE *fp)
{
    word_t word = { buff->t.leng-buff->read, buff->t.text+buff->read };
    word_puts(&word, width, fp);
}

buff_t *buff_shrink(buff_t *self, int offset)	/* shrink buffer */
{
    self->t.text += offset;
    self->t.leng -= offset;
    self->size -= offset;
    return self;
}

buff_t *buff_expand(buff_t *self, int offset)	/* expand buffer */
{
    self->t.text -= offset;
    self->t.leng += offset;
    self->size += offset;
    return self;
}

void buff_shift(buff_t *self, byte *start, size_t length)
{
    /* Shift buffer contents to delete the specified segment. */
    /* Implemented for deleting html comments.		      */
    byte *buff_end = self->t.text+self->t.leng;
    byte *move_end = start + length;
    if ( ! ((self->t.text <= start) && (move_end <= buff_end)) ) {
	assert("Invalid buff_shift() parameters.");
    }
    memmove(start, start+length, buff_end - move_end);
    self->read = 0;
    self->t.leng -= length;
    Z(self->t.text[self->t.leng]);		/* debug - remove me */
    return;
}
