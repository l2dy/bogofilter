/* $Id$ */

/*****************************************************************************

NAME:
   bogolexer.c -- runs bogofilter's lexer.

******************************************************************************/

/* imports */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <config.h>
#include "common.h"

#include "bool.h"
#include "charset.h"
#include "lexer.h"
#include "textblock.h"
#include "token.h"
#include "mime.h"

const char *progname = "bogolexer";

/* prevent larger inclusions */

run_t run_type = RUN_NORMAL;
const char *spam_header_name = SPAM_HEADER_NAME;

/* Function definitions */

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
	    "\t-h\thelp, this output.\n"
	    "\t-k y/n\t- kill html comments (yes or no).\n"
	    "\t-n\t- map non-ascii characters to '?'.\n"
	    "\t-v\t- set debug verbosity level.\n"
	    "\t-x list\t- set debug flags.\n"
	    "\t-I file\t- read message from file instead of stdin.\n"
	    "%s is part of the bogofilter package.\n", progname);
}

int main(int argc, char **argv)
{
    token_t t;
    int option;
    int count=0;

    fpin = stdin;
    dbgout = stderr;

    while ((option = getopt(argc, argv, ":hnpqvnx:I:k:")) != -1)
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
	case 'x':
	    set_debug_mask( optarg );
	    break;
	case 'k':
	    kill_html_comments = str_to_bool( optarg );
	    break;
	case 'I':
	    fpin = fopen( optarg, "r" );
	    if (fpin == NULL) {
		fprintf(stderr, "Can't read file '%s'\n", optarg);
		exit(2);
	    }
	    break;
	default:
	    usage();
	    exit(0);
	}

    textblocks = textblock_init();

    if (!passthrough)
    {
	if (quiet)
	    puts( "quiet mode.");
	else
	    puts("normal mode.");
    }

    init_charset_table("default", true);
    mime_reset();

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

    textblock_free(textblocks);

    return 0;
}
