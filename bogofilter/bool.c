/* $Id$ */

/*****************************************************************************

NAME:
   bool.c -- functions to support the "bool" type.

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#include <ctype.h>
#include <stdlib.h>

#include <config.h>
#include "common.h"

#include "bool.h"

bool str_to_bool(const char *str)
{
    char ch = toupper((unsigned char)*str);
    switch (ch)
    {
    case 'Y':		/* Yes */
    case 'T':		/* True */
    case '1':
	return (true);
    case 'N':		/* No */
    case 'F':		/* False */
    case '0':
	return (false);
    default:
	fprintf(stderr, "Invalid boolean value - %s\n", str);
	exit(2);
    }
}
