/* lexertest.c -- part of bogofilter */

/* imports */
#include <stdio.h>
#include <string.h>
#include "lexer.h"

/* exports */
int passthrough;

int main(int argc, char **argv)
{
    int	t, quiet = 0;

    if (argc >= 2 && argv[1] && 0 == strcmp(argv[1], "-h")) {
	puts("Usage: lexertest [options]");
	puts("Options are:");
	puts("  -h    help, this output.");
	puts("  -q    quiet mode, no tokens are printed.");
	exit(0);
    }

    if (argc >= 2 && argv[1] && 0 == strcmp(argv[1], "-q")) quiet=1;

    if (quiet) {
	puts("quiet mode.");
	while ((t = get_token()) > 0) { }
    } else {
	puts("normal mode.");
	while ((t = get_token()) > 0)
	{
	    (void) printf("get_token: %d '%s'\n", t, yylval);
	}
    }
    return 0;
}
