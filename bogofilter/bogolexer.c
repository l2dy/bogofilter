/* $Id$ */

/*****************************************************************************

NAME:
   bogolexer.c -- runs bogofilter's lexer.

******************************************************************************/

/* imports */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <config.h>
#include "common.h"

#include "lexer.h"

/* exports */
bool logflag;		/* '-l' */
bool quiet;		/* '-q' */
int  passthrough;	/* '-p' */
int  verbose;		/* '-v' */

const char *progname = "bogolexer";

const char *spam_header_name = SPAM_HEADER_NAME;

static void usage(void)
{
    fprintf(stderr, "Usage: %s [ -p | -q | -h ]\n", progname);
}

static void help(void)
{
    usage();
    fprintf(stderr,
	    "\t-p\tprint the tokens from stdin.\n"
	    "\t-q\tquiet mode, no tokens are printed.\n"
	    "\t-h\thelp, this output.\n"
	    "%s is part of the bogofilter package.\n", progname);
}

int main(int argc, char **argv)
{
    token_t t;
    int option;
    int count=0;

    while ((option = getopt(argc, argv, ":hpq")) != -1)
	switch (option) {
	case 'h':
	    help();
	    exit(0);
	    break;
	case 'q':
	    quiet = 1;
	    break;
	case 'p':
	    passthrough = 1;
	    break;
	default:
	    usage();
	    exit(0);
	}

    if (!passthrough)
    {
	if (quiet)
	    puts( "quiet mode.");
	else
	    puts("normal mode.");
    }

    while ((t = get_token()) > 0)
    {
	count += 1;
	if ( passthrough )
	    (void) printf("%s\n", yylval);
	else if (!quiet)
	    (void) printf("get_token: %d '%s'\n", t, yylval);
    }

    if ( !passthrough )
	printf( "%d tokens read.\n", count );

    return 0;
}
