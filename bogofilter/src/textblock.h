/* $Id$ */

/*****************************************************************************

NAME:
   textblock.h -- prototypes and definitions for textblock.c

******************************************************************************/

#ifndef	HAVE_TEXTBLOCK_H
#define	HAVE_TEXTBLOCK_H

typedef struct textdata_s {
    struct textdata_s *next;
    size_t             size;
    byte              *data;
} textdata_t;

typedef struct textblock_s {
    textdata_t *head;
    textdata_t *tail;
}  textblock_t;

extern textblock_t *textblocks;

textblock_t *textblock_init(void);
void textblock_add(textblock_t *textblock, const byte *text, size_t size);
void textblock_free(textblock_t *textblock);

#endif	/* HAVE_TEXTBLOCK_H */
