/* lexertest.c -- part of bogofilter */

/* imports */
#include <stdio.h>
#include "lexer.h"

/* exports */
int passthrough;

int main(void)
{
    int	t;

    while ((t = get_token()) > 0)
    {
	(void) printf("get_token: %d '%s'\n", t, yylval);
    }
    return 0;
}
