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

#include "charset.h"
#include "lexer.h"
#include "token.h"

/* exports */
bool logflag;				/* '-l' */
bool quiet;				/* '-q' */
int  passthrough;			/* '-p' */
int  verbose;				/* '-v' */
bool replace_nonascii_characters;	/* '-n' */

const char *progname = "bogolexer";

/* prevent larger inclusions */
const char *spam_header_name = SPAM_HEADER_NAME;
run_t run_type = RUN_NORMAL;

static void usage(void)
{
    fprintf(stderr, "Usage: %s [ -p | -q | -n | -h ]\n", progname);
}

static void help(void)
{
    usage();
    fprintf(stderr,
	    "\t-p\tprint the tokens from stdin.\n"
	    "\t-q\tquiet mode, no tokens are printed.\n"
	    "\t-n\tmap non-ascii characters to '?'.\n"
	    "\t-h\thelp, this output.\n"
	    "%s is part of the bogofilter package.\n", progname);
}

int main(int argc, char **argv)
{
    token_t t;
    int option;
    int count=0;

    while ((option = getopt(argc, argv, ":hnpqv")) != -1)
	switch (option) {
	case 'h':
	    help();
	    exit(0);
	    break;
	case 'q':
	    quiet = true;
	    break;
	case 'p':
	    passthrough = true;
	    break;
	case 'n':
	    replace_nonascii_characters = true;
	    break;
	case 'v':
	    verbose += 1;
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

    init_charset_table("default", true);

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
