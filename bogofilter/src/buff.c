/* $Id$ */

/** \file buff.c
 * implementation of the buff type, a buffer for arbitrary strings
 * and actually a superset of word_t
 *
 * \author David Relson <relson@osagesoftware.com>
 * \author Matthias Andree <matthias.andree@gmx.de> (buff_fgetsl)
 */

#include "common.h"

#include <stdlib.h>

#include "buff.h"
#include "fgetsl.h"
#include "xmalloc.h"

#define BOGO_ASSERT(expr, msg) \
    if (!(expr)) { \
	fprintf(stderr, "%s: %s:%d %s\n", progname, __FILE__, __LINE__, msg); \
	abort(); \
    }

/* Function Definitions */
buff_t *buff_init(buff_t *self, byte *buff, uint used, uint size)
{
    self->t.text = buff;
    self->t.leng = used;
    self->read = 0;
    self->size = size;
    return self;
}

buff_t *buff_new(byte *buff, uint used, uint size)
{
    buff_t *self = xmalloc(sizeof(buff_t));
    buff_init(self, buff, used, size);
    return self;
}

void buff_free(buff_t *self)
{
    xfree(self);
}

int buff_fgetsl(buff_t *self, FILE *in)
{
    uint readpos = self->t.leng;
    int readcnt = xfgetsl((char *)self->t.text+readpos, self->size-readpos, in, true);
    self->read = readpos;
    if (readcnt >= 0)
	self->t.leng += readcnt;
    return readcnt;
}

int buff_fgetsln(buff_t *self, FILE *in, uint maxlen)
{
    uint readpos = self->t.leng;
    int readcnt = xfgetsl((char *)self->t.text+readpos, min(self->size - readpos, maxlen), in, true);
    /* WARNING: do not add NUL termination, the size must be exact! */
    self->read = readpos;
    if (readcnt >= 0)
	self->t.leng += readcnt;
    return readcnt;
}

int buff_add(buff_t *self, word_t *in)
{
    uint readpos = self->t.leng;
    int readcnt = in->leng;
    uint new_size = self->t.leng + in->leng;
    if (new_size > self->size) {
	self->t.text = xrealloc(self->t.text, new_size);
	self->size = new_size;
    }
    self->read = readpos;
    self->t.leng += readcnt;
    memcpy(self->t.text+readpos, in->text, readcnt);
    Z(self->t.text[self->t.leng]);		/* for easier debugging - removable */

    return readcnt;
}

void buff_puts(const buff_t *self, uint width, FILE *fp)
{
    word_t word;
    word.leng = self->t.leng-self->read;
    word.text = self->t.text+self->read;
    word_puts(&word, width, fp);
}
