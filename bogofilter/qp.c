/* $Id$ */

/*****************************************************************************

NAME:
   qp.c -- decode quoted printable text

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#include <config.h>
#include "common.h"

#include "qp.h"

#define	HEX_TO_BIN(c) ((c <= '9') ? (c - '0') : (c +10 - 'A'))

int qp_decode(byte *buff, size_t size)
{
    byte *s = buff;		/* src */
    byte *d = buff;		/* dst */
    byte *e = buff + size;	/* end */

    while (s < e)
    {
	byte ch = *s++;
	if (ch == '=') {
	    ch = *s++;
	    if (ch != '\n') {
		byte cx = *s++;
		ch = HEX_TO_BIN(ch) << 4 | HEX_TO_BIN(cx);
	    }
	}
	*d++ = ch;
    }
    return d - buff;
}


