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

static byte base64_charset[] = {
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/" };
static byte base64_xlate[256];
static const byte base64_invalid = 0x7F;

static void base64_init(void)
{
    size_t i;
    for (i = 0; i < sizeof(base64_charset); i += 1) {
	byte c = base64_charset[i];
	base64_xlate[c] = (byte) i;
    }
    base64_xlate['='] = base64_invalid;
    return;
}

size_t base64_decode(word_t *word)
{
    static int table_set_up = 0;
    size_t count = 0;
    size_t size = word->leng;
    byte *s = word->text;		/* src */
    byte *d = word->text;		/* dst */

    if (!table_set_up) {
	table_set_up ++;
	base64_init();
    }

    while (size)
    {
	unsigned long v = 0;
	size_t i;
	unsigned int shorten = 0;
	while (size && (*s == '\r' || *s == '\n')) {
	    size--;
	    s++;
	}
	if (size < 4) break;
	for (i = 0; i < 4 && (size_t)i < size; i += 1) {
	    byte c = *s++;
	    byte t = base64_xlate[c];
	    if (t == base64_invalid) {
		shorten = 4 - i;
		i = 4;
		v >>= (shorten * 2);
		if (shorten == 2) s++;
		break;
	    }
	    v = v << 6 | t;
	}
	size -= i;
	for (i = 3 - shorten; i > 0; i -= 1) {
	    byte c = v & 0xFF;
	    d[i-1] = c;
	    v = v >> 8;
	}
	d += 3 - shorten;
	count += 3 - shorten;
    }
    *d = '\0';
    return count;
}
