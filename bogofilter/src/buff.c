/* $Id$ */

/*****************************************************************************

NAME:
   buff.c -- support for bogofilter's buff struct

AUTHORS:
   David Relson <relson@osagesoftware.com>
   Matthias Andree <matthias.andree@gmx.de> (buff_fgetsl)

******************************************************************************/

#include <stdlib.h>

#include "config.h"
#include "common.h"

#include "buff.h"
#include "fgetsl.h"
#include "xmalloc.h"

#define BOGO_ASSERT(expr, msg) if (!(expr)) { fprintf(stderr, "%s: %s:%d %s\n", progname, __FILE__, __LINE__, msg); abort(); }

/* Function Definitions */
buff_t *buff_init(buff_t *self, byte *buff, size_t used, size_t size)
{
    self->t.text = buff;
    self->t.leng = used;
    self->read = 0;
    self->size = size;
    return self;
}

buff_t *buff_new(byte *buff, size_t used, size_t size)
{
    buff_t *self = xmalloc(sizeof(buff_t));
    buff_init(self, buff, used, size);
    return self;
}

void buff_free(buff_t *self)
{
    xfree(self);
}

void buff_free_text(buff_t *self)
{
    xfree(self->t.text);
    xfree(self);
}

buff_t *buff_dup(const buff_t *self)
{
    buff_t *new = xmalloc(sizeof(buff_t));
    new->size = self->size;
    new->t.text = self->t.text;
    return new;
}

int buff_fgetsl(buff_t *self, FILE *in)
{
    int readpos = self->t.leng;
    int readcnt = xfgetsl((char *)self->t.text+readpos, self->size-readpos, in, 1);
    self->read = readpos;
    if (readcnt >= 0)
	self->t.leng += readcnt;
    return readcnt;
}

void buff_puts(const buff_t *self, size_t width, FILE *fp)
{
    word_t word;
    word.leng = self->t.leng-self->read;
    word.text = self->t.text+self->read;
    word_puts(&word, width, fp);
}

void buff_shift(buff_t *self, byte *start, size_t length)
{
    /* Shift buffer contents to delete the specified segment. */
    /* Implemented for deleting html comments.		      */
    byte *buff_end = self->t.text+self->t.leng;
    byte *move_end = start + length;

    BOGO_ASSERT((self->t.text <= start) && (move_end <= buff_end),
	   "Invalid buff_shift() parameters.");

    memmove(start, start+length, buff_end - move_end);
    self->t.leng -= length;
    Z(self->t.text[self->t.leng]);		/* for easier debugging - removable */
    return;
}
