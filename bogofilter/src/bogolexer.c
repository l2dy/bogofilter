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
#include "configfile.h"
#include "html.h"		/* for strict_check */
#include "lexer.h"
#include "textblock.h"
#include "token.h"
#include "mime.h"

#define PROGNAME "bogolexer"

const char *progname = PROGNAME;

/* prevent larger inclusions */

run_t run_type = RUN_NORMAL;
const char *spam_header_name = SPAM_HEADER_NAME;

const parm_desc sys_parms[] =
{
    { "stats_in_header",		CP_BOOLEAN,	{ (void *) &stats_in_header } },
    { "user_config_file",		CP_STRING,	{ &user_config_file } },
    { "block_on_subnets",		CP_BOOLEAN,	{ (void *) &block_on_subnets } },
    { "charset_default",		CP_STRING,	{ &charset_default } },
    { "replace_nonascii_characters",	CP_BOOLEAN,	{ (void *) &replace_nonascii_characters } },
    { "tag_header_lines",		CP_BOOLEAN,	{ (void *) &tag_header_lines } },
    { "strict_check",			CP_BOOLEAN,	{ (void *) &strict_check } },
    { NULL,				CP_NONE,	{ (void *) NULL } },
};

const parm_desc format_parms[] =
{
    { NULL, CP_NONE, { (void *) NULL } },
};

/* Function Prototypes */

static int process_args(int argc, char **argv);
static void initialize(FILE *fp);

/* Function Definitions */

static void usage(void)
{
    fprintf(stderr, "Usage: %s [ -p | -q | -n | -h ]\n", progname);
}

static void help(void)
{
    usage();
    fprintf(stderr,
	    "\n"
	    "\t-p\t- print the tokens from stdin.\n"
	    "\t-q\t- quiet mode, no tokens are printed.\n"
	    "\t-h\t- help, this output.\n"
	    "\t-n\t- map non-ascii characters to '?'.\n"
	    "\t-v\t- set verbosity level.\n"
	    "\t-c file\t- read specified config file.\n"
	    "\t-C\t- don't read standard config files.\n"
	    "\t-I file\t- read message from file instead of stdin.\n"
	    "\t-x list\t- set debug flags.\n"
	    "\t-D\t- direct debug output to stdout.\n"
	    "\n"
	    "%s (version %s) is part of the bogofilter package.\n", 
	    progname, version);
}

static int process_args(int argc, char **argv)
{
    int option;

    fpin = stdin;
    dbgout = stderr;

    while ((option = getopt(argc, argv, ":c:CDhI:npqTvx:")) != -1)
    {
	switch (option)
	{
	case ':':
	    fprintf(stderr, "Option -%c requires an argument.\n", optopt);
	    exit(2);

	case '?':
	    fprintf(stderr, "Unknown option -%c.\n", optopt);
	    exit(2);

	case 'c':
	    read_config_file(optarg, false, false, PR_COMMAND);
	/*@fallthrough@*/
	/* fall through to suppress reading config files */

	case 'C':
	    suppress_config_file = true;
	    break;

	case 'D':
	    dbgout = stdout;
	    break;

	case 'h':
	    help();
	    exit(0);

	case 'I':
	    fpin = fopen( optarg, "r" );
	    if (fpin == NULL) {
		fprintf(stderr, "Can't read file '%s'\n", optarg);
		exit(2);
	    }
	    break;

	case 'n':
	    replace_nonascii_characters = true;
	    break;

	case 'p':
	    passthrough = true;
	    break;

	case 'q':
	    quiet = true;
	    break;

	case 'T':
	    test += 1;
	    break;

	case 'v':
	    verbose += 1;
	    break;

	case 'x':
	    set_debug_mask( optarg );
	    break;

	default:
	    abort();
	    exit(2);
	}
    }
    if (optind < argc) {
	fprintf(stderr, "Extra arguments given, first: %s. Aborting.\n",
		argv[optind]);
	exit(2);
    }
    return 0;
}

int main(int argc, char **argv)
{
    token_t t;
    int count=0;

    process_args(argc, argv);
    process_config_files(false);

    textblocks = textblock_init();

    if (!passthrough)
    {
	if (quiet)
	    puts( "quiet mode.");
	else
	    puts("normal mode.");
    }

    initialize(fpin);
    got_from();	/* initialize */

    while ((t = get_token()) > 0)
    {
	count += 1;
	if (passthrough) {
	    fprintf(stdout, "%s\n", yylval->text);
	}
	else if (!quiet)
	    printf("get_token: %d '%s`\n", t, yylval->text);
    }

    if ( !passthrough )
	printf( "%d tokens read.\n", count );

    textblock_free(textblocks);

    return 0;
}

static void initialize(FILE *fp)
{
    init_charset_table(charset_default, true);
    mime_reset();
    token_init();
    lexer_v3_init(fp);
}
