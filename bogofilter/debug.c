/* $Id$ */

/*****************************************************************************

NAME:
   debug.c - shared debug functions

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"

int debug_mask = DEBUG_NONE;

void set_debug_mask(const char *mask)
{
    char ch;
    const char *maskbits = BIT_NAMES;
    for (ch = tolower((unsigned char)*mask); ch != '\0'; ch = *++mask)
    {
	/*@-shiftnegative@*/
	if (strchr(maskbits, ch) != NULL)
	    debug_mask |= (1 << (ch - 'a'));
	/*@=shiftnegative@*/
	else
	{
	    (void)fprintf(stderr, "unknown mask option '%c'\n", ch);
	    exit(2);
	}
    }
}
