/* $Id$ */

/*****************************************************************************

NAME:
   base64.c -- decode quoted printable text

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#include <config.h>
#include "common.h"

#include "base64.h"

#define	HEX_TO_BIN(c) ((c <= '9') ? (c - '0') : (c +10 - 'A'))

byte base64_charset[] = { "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/" };
byte base64_xlate[256];

void base64_init(void)
{
    size_t i;
    for (i = 0; i < sizeof(base64_charset); i += 1) {
	byte c = base64_charset[i];
	base64_xlate[c] = (byte) i;
    }
    return;
}

int base64_decode(byte *buff, size_t size)
{
    size_t count = 0;
    byte *s = buff, *d = buff;

    while (size > 4)
    {
	int v = 0;
	int i;
	for (i = 0; i < 4; i += 1) {
	    byte c = *s++;
	    byte t = base64_xlate[c];
	    v = v << 6 | t;
	}
	for (i = 2; i >= 0; i -= 1) {
	    byte c = v & 0xFF;
	    d[i] = c;
	    v = v >> 8;
	}
	d += 3;
	count += 3;
	size -= 4;
    }
    *d = '\0';
    return count;
}
