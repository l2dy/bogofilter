/* $Id$ */

/*****************************************************************************

NAME:
   debug.c - shared debug functions

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"

bit_t debug_mask = DEBUG_NONE;

void set_debug_mask( char *mask )
{
    char ch;
    char *maskbits = MASKNAMES;
    for (ch = *mask; ch != '\0'; ch = *++mask)
    {
	/*@-shiftnegative@*/
	char *pos = strchr(maskbits, ch);
	if (pos != NULL)
	    debug_mask |= (1 << (pos-maskbits));
	/*@=shiftnegative@*/
	else
	{
	    (void)fprintf(stderr, "unknown mask option '%c'\n", ch);
	    exit(2);
	}
    }
}
