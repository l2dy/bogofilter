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

int uudecode(byte *buff, size_t size)
{
    size_t count = 0;
    byte *s = buff, *d = buff, *e = buff+size;
    int out = (*s++ & 0x7f) - 0x20;

    /* don't process lines without leading count character */
    if (out < 0)
	return size;

    /* don't process begin and end lines */
    if ((strncasecmp(buff, "begin ", 6) == 0) ||
	(strncasecmp(buff, "end", 3) == 0))
	return size;

    while (s < e - 4)
    {
	int v = 0;
	int i;
	for (i = 0; i < 4; i += 1) {
	    byte c = *s++;
	    v = v << 6 | (c - 0x20);
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
