/* $Id$ */

/*****************************************************************************

NAME:
   buff.h -- constants and declarations for buff.c

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#ifndef	BUFF_H
#define	BUFF_H

#include <stdio.h>

#include "word.h"

typedef struct {
    word_t t;
    size_t pos;		/* read/write position  */
    size_t read;	/* start of last read   */
    size_t size;	/* capacity   */
} buff_t;

extern buff_t  *buff_new(byte *buff, size_t used, size_t size);
extern buff_t  *buff_shrink(buff_t *self, int offset);
extern buff_t  *buff_expand(buff_t *self, int offset);
extern void 	buff_free(buff_t *self);
extern buff_t  *buff_dup(const buff_t *self);
extern int 	buff_cmp(const buff_t *t1, const buff_t *t2);
extern int	buff_fgetsl(buff_t *self, FILE *);
extern void 	buff_puts(const buff_t *self, size_t width, FILE *fp);

extern void 	buff_shift(buff_t *self, byte *start, size_t length);

#endif	/* BUFF_H */

