/* $Id$ */

/** \file word.h
 * constants and declarations for word.c
 *
 * \author David Relson <relson@osagesoftware.com>
 */

#ifndef	WORD_H
#define	WORD_H

#include "bftypes.h"

/** Structure to store a string of arbitrary content, including '\\0'
 * bytes.
 * \attention to lessen malloc/free calls, this struct and the data
 * are allocated in one chunk. */
typedef struct {
    uint leng;		/** length of string in \a text */
    byte *text;		/** pointer to the string */
} word_t;

/** create a new word_t from the \a leng bytes at address \a text */
extern word_t  *word_new(const byte *text, uint leng);

/** deallocate memory allocated for \a self */
extern void 	word_free(word_t *self);

/** create a newly word_t to form a deep copy of \a self */
extern word_t  *word_dup(const word_t *self);

/** copy the content of \a src into \a dst. */
extern word_t  *word_cpy(word_t *dst, const word_t *src);

/** compare \a w1 and \a w2 with memcmp() */
extern int 	word_cmp(const word_t *w1, const word_t *w2);

/** create a new word_t that is the concatenation of \a w1 and \a w2 */
extern word_t  *word_concat(const word_t *w1, const word_t *w2);

/** output \a self onto the stdio stream \a fp, formatted to \a width
 * characters. */
extern void 	word_puts(const word_t *self,
	uint width, /**< if 0, use actual width, if > 0 then either
		     *   truncate the string or fill it with blanks to print
		     *   exactly \a width characters */
	FILE *fp);

/** Compare word \a w to string \a s. */
extern int	word_cmps(const word_t *w, const char *s);

#endif	/* WORD_H */
