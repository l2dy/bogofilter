/* $Id$ */

/*****************************************************************************

NAME:
   buff.h -- constants and declarations for buff.c

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#ifndef	BUFF_H
#define	BUFF_H

#include "word.h"

typedef struct {
    word_t t;
    size_t pos;		/* read/write position  */
    size_t read;	/* start of last read   */
    size_t size;	/* capacity   */
} buff_t;

extern buff_t  *buff_new(byte *buff, size_t used, size_t size);
extern buff_t  *buff_shrink(buff_t *t, int offset);
extern buff_t  *buff_expand(buff_t *t, int offset);
extern void 	buff_free(buff_t *t);
extern buff_t  *buff_dup(const buff_t *t);
extern int 	buff_cmp(const buff_t *t1, const buff_t *t2);
extern void 	buff_puts(const buff_t *t, FILE *fp);

#endif	/* BUFF_H */

