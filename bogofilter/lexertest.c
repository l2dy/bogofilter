/* lexertest.c -- part of bogofilter */

/* imports */
#include <stdio.h>
#include "lexer.h"

extern char *yytext;

/* exports */
int passthrough;

int main(void)
{
    int	t;

    while ((t = get_token()) > 0)
    {
	(void) printf("get_token: %d '%s'\n", t, yytext);
    }
    return 0;
}
