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

static int hex_to_bin(byte c) {
    switch(c) {
	case '0': return 0;
	case '1': return 1;
	case '2': return 2;
	case '3': return 3;
	case '4': return 4;
	case '5': return 5;
	case '6': return 6;
	case '7': return 7;
	case '8': return 8;
	case '9': return 9;
	case 'a': case 'A': return 10;
	case 'b': case 'B': return 11;
	case 'c': case 'C': return 12;
	case 'd': case 'D': return 13;
	case 'e': case 'E': return 14;
	case 'f': case 'F': return 15;
	default: return -1;
    }
}

size_t qp_decode(word_t *word)
{
    size_t size = word->leng;
    byte *s = word->text;	/* src */
    byte *d = word->text;	/* dst */
    byte *e = s + size;		/* end */

    while (s < e)
    {
	byte ch = *s++;
	int x, y;
	if (ch == '=' && s <= e && s[0] == '\n') {
	    /* continuation line, trailing = */
	    s++;
	    continue;
	}
	if (ch == '=' && s + 2 < e && (y = hex_to_bin(s[0])) >= 0 &&
		    (x = hex_to_bin(s[1])) >= 0) {
	    /* encoded character */
		ch = y << 4 | x;
		s += 2;
	}
	*d++ = ch;
    }
    *d = '\0';
    return d - word->text;
}
