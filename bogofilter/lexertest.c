/* lexertest.c -- part of bogofilter */

/* imports */
#include <stdio.h>
#include <string.h>
#include "lexer.h"

/* exports */
int passthrough;

void help(void);

int main(int argc, char **argv)
{
    char *arg;
    int	t;
    int count=0;
    int quiet = 0;
    int passthru = 0;

    while ((arg = *++argv))
    {
	if (strcmp(arg, "-h") == 0)
	    help();
	quiet += strcmp(arg, "-q") == 0;
	passthru += strcmp(arg, "-p") == 0;
    }

    if (!passthru)
	if (quiet)
	    puts( "quiet mode.");
	else
	    puts("normal mode.");

    while ((t = get_token()) > 0)
    {
	count += 1;
	if ( passthru )
	    (void) printf("%s\n", yylval);
	else if (!quiet)
	    (void) printf("get_token: %d '%s'\n", t, yylval);
    }
    if ( !passthru )
	printf( "%d tokens read.\n", count );

    return 0;
}

void help(void)
{
    puts("Usage: lexertest [options]");
    puts("Options are:");
    puts("  -h    help, this output.");
    puts("  -q    quiet mode, no tokens are printed.");
    puts("  -p    print the tokens.");
    exit(0);
}
