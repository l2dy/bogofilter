/* $Id$ */

/*****************************************************************************

NAME:
   bogolexer.c -- runs bogofilter's lexer.

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#include "common.h"

#include <ctype.h>
#include "getopt.h"
#include <stdlib.h>
#include <string.h>

#include "bogoconfig.h"
#include "bogoreader.h"
#include "bool.h"
#include "charset.h"
#include "configfile.h"
#include "lexer.h"
#include "mime.h"
#include "textblock.h"
#include "token.h"
#include "format.h"
#include "xstrdup.h"

const char *progname = "bogolexer";

/* prevent larger inclusions */

const char *spam_header_name = SPAM_HEADER_NAME;

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
	    "\t-H\t- disables header line tagging.\n"
	    "\t-I file\t- read message from file instead of stdin.\n"
	    "\t-x list\t- set debug flags.\n"
	    "\t-D\t- direct debug output to stdout.\n");
    fprintf(stderr,
	    "\n"
	    "%s (version %s) is part of the bogofilter package.\n", 
	    progname, version);
}

static void print_version(void)
{
    (void)fprintf(stderr,
		  "%s version %s\n"
		  "Copyright (C) 2002-2004 David Relson\n\n"
		  "%s comes with ABSOLUTELY NO WARRANTY.  "
		  "This is free software, and\nyou are welcome to "
		  "redistribute it under the General Public License.  "
		  "See\nthe COPYING file with the source distribution for "
		  "details.\n"
		  "\n", 
		  progname, version, PACKAGE);
}

struct option long_options[] = {
    { "config-file",			N, 0, 'c' },
    { "no-config-file",			N, 0, 'C' },
    { "debug-flags",			R, 0, 'x' },
    { "debug-to-stdout",		N, 0, 'D' },
    { "no-header-tags",			N, 0, 'H' },
    { "input-file",			N, 0, 'I' },
    { "output-file",			N, 0, 'O' },
    { "query",				N, 0, 'Q' },
    { "verbosity",			N, 0, 'v' },
    { "block_on_subnets",		R, 0, O_BLOCK_ON_SUBNETS },
    { "bogofilter_dir",			R, 0, O_IGNORE },
    { "charset_default",		R, 0, O_IGNORE },
    { "ham_cutoff",			R, 0, O_IGNORE },
    { "header_format",			R, 0, O_IGNORE },
    { "log_header_format",		R, 0, O_IGNORE },
    { "log_update_format",		R, 0, O_IGNORE },
    { "min_dev",			R, 0, O_IGNORE },
    { "replace_nonascii_characters",	R, 0, 'n' },
    { "robs",				R, 0, O_IGNORE },
    { "robx",				R, 0, O_IGNORE },
    { "spam_cutoff",			R, 0, O_IGNORE },
    { "spam_header_name",		R, 0, O_IGNORE },
    { "spam_subject_tag",		R, 0, O_IGNORE },
    { "spamicity_formats",		R, 0, O_IGNORE },
    { "spamicity_tags",			R, 0, O_IGNORE },
    { "stats_in_header",		R, 0, O_IGNORE },
    { "terse",				R, 0, O_IGNORE },
    { "terse_format",			R, 0, O_IGNORE },
    { "thresh_update",			R, 0, O_IGNORE },
    { "timestamp",			R, 0, O_IGNORE },
    { "unsure_subject_tag",		R, 0, O_IGNORE },
    { "user_config_file",		R, 0, O_USER_CONFIG_FILE },
    { NULL,				0, 0, 0 }
};

static bool get_bool(const char *name, const char *arg)
{
    bool b = str_to_bool(arg);
    if (DEBUG_CONFIG(2))
	fprintf(dbgout, "%s -> %s\n", name,
		b ? "Yes" : "No");
    return b;
}

static char *get_string(const char *name, const char *arg)
{
    char *s = xstrdup(arg);
    if (DEBUG_CONFIG(2))
	fprintf(dbgout, "%s -> '%s'\n", name, s);
    return s;
}

#define	OPTIONS	":c:CDhHI:npP:qvVx:X:m"

/** These functions process command line arguments.
 **
 ** They are called to perform passes 1 & 2 of command line switch processing.
 ** The config file is read in between the two function calls.
 **
 ** The functions will exit if there's an error, for example if
 ** there are leftover command line arguments.
 */

static void process_arglist(int argc, char **argv)
{
    int option;

    fpin = stdin;
    dbgout = stderr;

    while (1)
    {
	int option_index = 0;
	int this_option_optind = optind ? optind : 1;
	const char *name;

	option = getopt_long(argc, argv, OPTIONS,
			     long_options, &option_index);

	if (option == -1)
	    break;

	name = (option_index == 0) ? argv[this_option_optind] : long_options[option_index].name;
	process_arg(option, name, optarg, PR_COMMAND, PASS_1_CLI);
    }

    if (optind < argc) {
	fprintf(stderr, "Extra arguments given, first: %s. Aborting.\n",
		argv[optind]);
	exit(EX_ERROR);
    }
}

void process_arg(int option, const char *name, const char *val, priority_t precedence, arg_pass_t pass)
{
    pass = 0;		/* suppress compiler warning */

    switch (option)
    {
    case ':':
	fprintf(stderr, "Option -%c requires an argument.\n", optopt);
	exit(EX_ERROR);

    case '?':
	fprintf(stderr, "Unknown option '%s'.\n", name);
	break;

    case 'c':
	read_config_file(val, false, false, precedence);
	/*@fallthrough@*/
	/* fall through to suppress reading config files */

    case 'C':
	suppress_config_file = true;
	break;

    case O_USER_CONFIG_FILE:
	user_config_file = get_string(name, val);
	break;

    case 'D':
	dbgout = stdout;
	break;

    case 'h':
	help();
	exit(EX_OK);

    case 'H':
	header_line_markup = false;
	break;

    case 'I':
	bogoreader_name(val);
	break;

    case 'n':
	replace_nonascii_characters = true;
	break;

    case O_REPLACE_NONASCII_CHARACTERS:
	replace_nonascii_characters = get_bool(name, val);	
	break;

    case 'p':
	passthrough = true;
	break;

    case 'q':
	quiet = true;
	break;

    case 'v':
	verbose += 1;
	break;

    case 'V':
	print_version();
	exit(EX_OK);

    case 'x':
	set_debug_mask(val);
	break;

    case 'X':
	set_bogotest(val);
	break;
	
    case O_BLOCK_ON_SUBNETS:
	block_on_subnets = get_bool(name, val);
	break;

    default:
	/* config file options:
	**  ok    - if from config file
	**  error - if on command line
	*/
	if (pass == PASS_2_CFG) {
	    fprintf(stderr, "Invalid option '%s'.\n", name);
	    exit(EX_ERROR);
	}
    }
}

static int count=0;

int main(int argc, char **argv)
{
    token_t t;

    mbox_mode = true;		/* to allow multiple messages */

    process_arglist(argc, argv);
    process_config_files(false);

    textblock_init();

    if (!passthrough)
    {
	if (quiet)
	    puts( "quiet mode.");
	else
	    puts("normal mode.");
    }

    bogoreader_init(argc, argv);

    while ((*reader_more)()) {
	lexer_init();

	while ((t = get_token()) != NONE)
	{
	    count += 1;
	    if (passthrough) {
		fprintf(stdout, "%s\n", yylval->text);
	    }
	    else if (!quiet)
		printf("get_token: %d \"%s\"\n", (int)t, yylval->text);
	}
    }

    if ( !passthrough )
	printf( "%d tokens read.\n", count );

    textblock_free();

    MEMDISPLAY;

    return 0;
}

