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
    word_t		t;
    unsigned int	read;	/* start of last read */
    unsigned int   size;	/* capacity */
} buff_t;

extern buff_t  *buff_new(byte *buff, unsigned int used, unsigned int size);
extern buff_t  *buff_init(buff_t *self, byte *buff, unsigned int used, unsigned int size);
extern void 	buff_free(buff_t *self);
extern void	buff_free_text(buff_t *self);
extern int	buff_add(buff_t *self, word_t *in);
extern buff_t  *buff_dup(const buff_t *self);
extern int 	buff_cmp(const buff_t *t1, const buff_t *t2);
extern int	buff_fgetsl(buff_t *self, FILE *);
extern int	buff_fgetsln(buff_t *self, FILE *, unsigned int);
extern void 	buff_puts(const buff_t *self, unsigned int width, FILE *fp);

extern void 	buff_shift(buff_t *self, byte *start, unsigned int length);

#endif	/* BUFF_H */

