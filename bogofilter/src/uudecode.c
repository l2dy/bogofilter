/* $Id$ */

/*****************************************************************************

NAME:
   uudecode.c -- decode uuencoded text

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#include <config.h>
#include "common.h"

#include "uudecode.h"

int uudecode(word_t *word)
{
    size_t size = word->leng;
    size_t count = 0;
    byte *b = word->text;		/* beg */
    byte *s = b;			/* src */
    byte *d = b;			/* dst */
    byte *e = b+size;			/* end */
    int out = (*s++ & 0x7f) - 0x20;

    /* don't process lines without leading count character */
    if (out < 0)
	return size;

    /* don't process begin and end lines */
    if ((strncasecmp((const char *)b, "begin ", 6) == 0) ||
	(strncasecmp((const char *)b, "end",    3) == 0))
	return size;

    while (s < e - 4)
    {
	int v = 0;
	int i;
	for (i = 0; i < 4; i += 1) {
	    byte c = *s++;
	    v = v << 6 | ((c - 0x20) & 0x3F);
	}
	for (i = 2; i >= 0; i -= 1) {
	    byte c = v & 0xFF;
	    d[i] = c;
	    v = v >> 8;
	}
	d += 3;
	count += 3;
    }
    while (s < e) 
    {
	*d++ = *s++;
	count += 1;
    }
    *d = '\0';
    return count;
}
