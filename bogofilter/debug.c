/* $Id$ */

/*****************************************************************************

NAME:
   debug.c - shared debug functions

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include "debug.h"

bit_t debug_mask = DEBUG_NONE;

void set_debug_mask( char *mask )
{
    char ch;
    char *maskbits = MASKNAMES;
    for (ch = *mask; ch != '\0'; ch = *++mask)
    {
	char *pos = index(maskbits, ch);
	if (pos != NULL)
	    debug_mask |= ( 1 << (pos-maskbits) );
	else
	{
	    fprintf( stderr, "unknown mask option '%c'\n", ch );
	    exit(2);
	}
    }
}

#ifdef	MAIN
typedef struct mask_char_to_symbol_s {
    char  *str;
    bit_t bit;
}  mask_char_to_symbol_t;

mask_char_to_symbol_t char_to_symbol[] =
{
    { "g",	BIT_GENERAL },
    { "s",	BIT_SPAMICITY },
    { "d",	BIT_DATABASE },
    { "l",	BIT_LEXER },
    { "w",	BIT_WORDLIST }
};

int main(int argc, char **argv)
{
    int i;
    for (i=0; i<sizeof(char_to_symbol)/sizeof(char_to_symbol[0]); i += 1)
    {
	mask_char_to_symbol_t *ptr = char_to_symbol + i;
	set_debug_mask( ptr->str );
	if ( (debug_mask & ptr->bit) != ptr->bit )
	{
	    fprintf(stderr, "debug_mask for '%s' is wrong.\n", ptr->str);
	    exit(2);
	}
    }
    printf("All O.K.\n");
    return 0;
}
#endif
