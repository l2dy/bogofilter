/* $Id$ */

/*****************************************************************************

NAME:
   bogolexer.c -- runs bogofilter's lexer.

******************************************************************************/

/*
** RCS file: /cvsroot/bogofilter/bogofilter/lexertest.c,v
** Working file: lexertest.c
** head: 1.5
** branch:
** locks: strict
** access list:
** symbolic names:
** 	release-0_7_6: 1.4
** 	bogofilter-beta-0_7_6_1: 1.4
** 	bogofilter-0_7_6: 1.4.0.2
** 	bogofilter-0_7_5: 1.3.0.2
** keyword substitution: kv
** total revisions: 5;	selected revisions: 5
** description:
** ----------------------------
** revision 1.5
** date: 2002/10/28 21:29:31;  author: relson;  state: Exp;  lines: +37 -16
** Added a passthrough option "-p".  Useful for simply seeing the words
** or for piping to bogoutil to get spam/good counts.
** ----------------------------
** revision 1.4
** date: 2002/10/25 01:38:36;  author: m-a;  state: Exp;  lines: +8 -0
** Add -h for help.
** ----------------------------
** revision 1.3
** date: 2002/10/09 23:40:51;  author: m-a;  state: Exp;  lines: +14 -5
** Implement a fast -q (quiet) mode to suppress the token output. Useful
** for benchmarks.
** ----------------------------
** revision 1.2
** date: 2002/10/08 03:29:57;  author: mmhoffman;  state: Exp;  lines: +1 -3
** Fix interface to lexer bug; use yylval in place of yytext.
** ----------------------------
** revision 1.1
** date: 2002/10/07 15:41:45;  author: m-a;  state: Exp;
** Move lexertest main() function from lexer.l into a separate file.
** This way, lexertest and bogofilter share the same lexer code, cutting down
** compilation times and saving space.
** =============================================================================
*/

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
