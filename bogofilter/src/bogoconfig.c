/* $Id$ */

/*****************************************************************************

NAME:
   bogoconfig.c -- process config file parameters

   2003-02-12 - split out from config.c	

AUTHOR:
   David Relson <relson@osagesoftware.com>

CONTRIBUTORS:
   David Saez	-O option, helps embedding into Exim.

******************************************************************************/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>

#include <config.h>
#include "common.h"
#include "globals.h"

#include "bogoconfig.h"
#include "bogofilter.h"
#include "bool.h"
#include "charset.h"
#include "configfile.h"
#include "maint.h"
#include "error.h"
#include "find_home.h"
#include "html.h"
#include "format.h"
#include "lexer.h"
#include "maint.h"
#include "paths.h"
#include "wordlists.h"
#include "xatox.h"
#include "xmalloc.h"
#include "xstrdup.h"
#include "xstrlcpy.h"

/* includes for scoring algorithms */
#include "method.h"
#ifdef	ENABLE_GRAHAM_METHOD
#include "graham.h"
#endif
#ifdef	ENABLE_ROBINSON_METHOD
#include "robinson.h"
#endif
#ifdef	ENABLE_ROBINSON_FISHER
#include "fisher.h"
#endif

#ifndef	DEBUG_CONFIG
#define DEBUG_CONFIG(level)	(verbose > level)
#endif

/*---------------------------------------------------------------------------*/

/* Global variables */

char outfname[PATH_LEN] = "";

run_t run_type = RUN_UNKNOWN;

const char *logtag = NULL;

/* define default */
#if defined (ENABLE_ROBINSON_FISHER)
#define AL_DEFAULT AL_FISHER
#elif defined (ENABLE_ROBINSON_METHOD)
#define AL_DEFAULT AL_ROBINSON
#elif defined (ENABLE_GRAHAM_METHOD)
#define AL_DEFAULT AL_GRAHAM
#else
#error No algorithms compiled in. See configure --help.
#endif

enum algorithm_e {
    AL_GRAHAM='g',
    AL_ROBINSON='r',
    AL_FISHER='f'
};

/* Local variables and declarations */

static enum algorithm_e algorithm = AL_DEFAULT;
static bool cmd_algorithm = false;		/* true if specified on command line */

static void display_tag_array(const char *label, const char **array);

static bool config_algorithm(const unsigned char *s);
static bool select_algorithm(const unsigned char ch, bool cmdline);

static void process_args(int argc, char **argv, int pass);
static void comma_parse(char opt, const char *arg, double *parm1, double *parm2);

/* externs for query_config() */

extern double robx, robs;
extern const char *header_format;
extern const char *terse_format;
extern const char *log_update_format;
extern const char *log_header_format;
extern const char *spamicity_tags;
extern const char *spamicity_formats;

/*---------------------------------------------------------------------------*/

/* Notes:
**
**	For options specific to algorithms, the algorithm files contain
**		a parm_desc struct giving their particulars.
**
**	The addr field is NULL for options processed by special functions
**		and on dummy entries for options private to algorithms
**		so config.c won't generate an error message.
*/

const parm_desc sys_parms[] =
{
    { "stats_in_header",  CP_BOOLEAN,	{ (void *) &stats_in_header } },
    { "user_config_file", CP_STRING,	{ &user_config_file } },

    { "algorithm",  	  CP_FUNCTION,	{ (void *) &config_algorithm } },
    { "bogofilter_dir",	  CP_DIRECTORY,	{ &directory } },
    { "wordlist",	  CP_FUNCTION,	{ (void *) &configure_wordlist } },
    { "update_dir",	  CP_STRING,	{ &update_dir } },

    { "min_dev",	  CP_DOUBLE,	{ (void *) &min_dev } },
    { "spam_cutoff",	  CP_DOUBLE,	{ (void *) &spam_cutoff } },
    { "thresh_stats",	  CP_DOUBLE,	{ (void *) &thresh_stats } },
#ifdef ENABLE_GRAHAM_METHOD
    { "thresh_index",	  CP_INTEGER,	{ (void *) NULL } },	/* Graham */
#endif
#ifdef ENABLE_ROBINSON_METHOD
    { "thresh_rtable",	  CP_DOUBLE,	{ (void *) NULL } },	/* Robinson */
    { "robx",		  CP_DOUBLE,	{ (void *) NULL } },	/* Robinson */
    { "robs",		  CP_DOUBLE,	{ (void *) NULL } },	/* Robinson */
#endif
#ifdef ENABLE_ROBINSON_FISHER
    { "ham_cutoff",	  CP_FUNCTION,	{ (void *) NULL } },	/* Robinson-Fisher */
#endif
    { "block_on_subnets", CP_BOOLEAN,	{ (void *) &block_on_subnets } },
    { "charset_default",  CP_STRING,	{ &charset_default } },
    { "datestamp_tokens",		CP_BOOLEAN, { (void *) &datestamp_tokens } },
    { "replace_nonascii_characters",	CP_BOOLEAN, { (void *) &replace_nonascii_characters } },
    { "db_cachesize",	  CP_INTEGER,	{ (void *) &db_cachesize } },
    { "tag_header_lines", CP_BOOLEAN,	{ (void *) &tag_header_lines } },
    { "strict_check", 	  CP_BOOLEAN,	{ (void *) &strict_check } },
    { NULL,		  CP_NONE,	{ (void *) NULL } },
};

void process_args_and_config_file(int argc, char **argv, bool warn_on_error)
{
    process_args(argc, argv, 1);
    process_config_files(warn_on_error);
    process_args(argc, argv, 2);

    if (!twostate && !threestate) {
	twostate = ham_cutoff < EPS;
	threestate = !twostate;
    }

    /* directories from command line and config file are already handled */

    if (setup_wordlists(NULL, PR_ENV_BOGO) != 0)
	exit(2);
    if (setup_wordlists(NULL, PR_ENV_HOME) != 0)
	exit(2);

    stats_prefix= stats_in_header ? "  " : "# ";

    if (DEBUG_CONFIG(0))
	fprintf(dbgout, "stats_prefix: '%s'\n", stats_prefix);

    return;
}

void comma_parse(char opt, const char *arg, double *parm1, double *parm2)
{
    char *parse = xstrdup(arg);
    char *p1, *p2;
    bool ok = true;
    if (parse[0] == ',') {
	p1 = NULL;
	p2 = &parse[1];
    } else {
	p1 = strtok(parse, ",");
	p2 = strtok(NULL, "");
    }
    if (p1) ok = ok && xatof(parm1, p1);
    if (p2) ok = ok && xatof(parm2, p2);
    if (!ok) {
	fprintf(stderr, "Cannot parse -%c option argument '%s'.\n", opt, arg);
    }
    xfree(parse);
}

static bool config_algorithm(const unsigned char *s)
{
    return select_algorithm(tolower(*s), false);
}

static bool select_algorithm(const unsigned char ch, bool cmdline)
{
    enum algorithm_e al = ch;

    /* if algorithm specified on command line, ignore value from config file */
    if (cmd_algorithm && !cmdline)
	return true;

    algorithm = al;
    cmd_algorithm |= cmdline;

    switch (al)
    {
#ifdef ENABLE_GRAHAM_METHOD
    case AL_GRAHAM:
	method = (method_t *) &graham_method;
	break;
#endif
#ifdef ENABLE_ROBINSON_METHOD
    case AL_ROBINSON:
	method = (method_t *) &rf_robinson_method;
	break;
#endif
#ifdef ENABLE_ROBINSON_FISHER
    case AL_FISHER:
	method = (method_t *) &rf_fisher_method;
	break;
#endif
    default:
	print_error(__FILE__, __LINE__, "Algorithm '%c' not supported.\n", al);
	return false;
    }
    usr_parms = method->config_parms;
    return true;
}

static int validate_args(void)
{
    bool registration, classification;

/*  flags '-s', '-n', '-S', or '-N', are mutually exclusive of flags '-p', '-u', '-e', and '-R'. */
    classification = (run_type & (RUN_NORMAL | RUN_UPDATE)) || passthrough || nonspam_exits_zero || (Rtable != 0);
    registration   = (run_type & (REG_SPAM | REG_GOOD | UNREG_SPAM | UNREG_GOOD)) != 0;

    if (*outfname && !passthrough)
    {
	(void)fprintf(stderr,
		      "Warning: Option -O %s has no effect without -p\n",
		      outfname);
    }
    
    if (registration && classification)
    {
	(void)fprintf(stderr, 
		      "Error:  Invalid combination of options.\n"
		      "\n"
		      "    Options '-s', '-n', '-S', and '-N' are used when registering words.\n"
		      "    Options '-p', '-u', '-e', and '-R' are used when classifying messages.\n"
		      "    The two sets of options may not be used together.\n"
		      "    \n"
#ifdef	GRAHAM_AND_ROBINSON
		      "    Options '-g', '-r', '-l', '-d', '-x', and '-v' may be used with either mode.\n"
#else
		      "    Options '-l', '-d', '-x', and '-v' may be used with either mode.\n"
#endif
	    );
	return 2;
    }

    return 0;
}

static void help(void)
{
    (void)fprintf(stderr,
		  "%s version %s\n"
		  "\n"
		  "Usage: %s [options] < message\n"
		  "\t-h\t- print this help message.\n"
		  "\t-d path\t- specify directory for wordlists.\n"
#ifdef	GRAHAM_AND_ROBINSON
#ifdef	ENABLE_GRAHAM_METHOD
		  "\t-g\t- select Graham spam calculation method.\n"
#endif
#ifdef	ENABLE_ROBINSON_METHOD
		  "\t-r\t- select Robinson spam calculation method.\n"
#endif
#ifdef	ENABLE_ROBINSON_FISHER
		  "\t-f\t- select Fisher spam calculation method (default).\n"
#endif
#endif
		  ,
		  PACKAGE, version, PACKAGE);
    (void)fprintf(stderr,
		  "\t-2\t- set binary classification mode (yes/no).\n"
		  "\t-3\t- set ternary classification mode (yes/no/unsure).\n"
		  "\t-p\t- passthrough.\n"
		  "\t-I file\t- read message from 'file' instead of stdin.\n"
		  "\t-O file\t- save message to 'file' in passthrough mode.\n"
		  "\t-e\t- in -p mode, exit with code 0 when the mail is not spam.\n"
		  "\t-s\t- register message(s) as spam.\n"
		  "\t-n\t- register message(s) as non-spam.\n"
		  "\t-m val [,val]\t- set user defined min_dev and robs values.\n");
    (void)fprintf(stderr,
		  "\t-o val [,val]\t- set user defined spam and non-spam cutoff values.\n"
		  "\t-u\t- classify message as spam or non-spam and register accordingly.\n"
		  "\t-S\t- unregister message(s) from spam list.\n"
		  "\t-N\t- unregister message(s) from non-spam list.\n"
		  "\t-R\t- print an R data frame.\n");
    (void)fprintf(stderr,
		  "\t-v\t- set debug verbosity level.\n"
		  "\t-V\t- print version information and exit.\n"
		  "\t-c file\t- read specified config file.\n"
		  "\t-C\t- don't read standard config files.\n"
		  "\t-q\t- quiet - don't print warning messages.\n"
		  "\t-l\t- write messages to syslog.\n");
    (void)fprintf(stderr,
		  "\t-L tag\t- specify the tag value for log messages.\n"
		  "\t-F\t- force printing of spamicity numbers.\n"
		  "\t-x list\t- set debug flags.\n"
		  "\t-D\t- direct debug output to stdout.\n"
		  "\n"
		  "bogofilter is a tool for classifying email as spam or non-spam.\n"
		  "\n"
		  "For updates and additional information, see\n"
		  "URL: http://bogofilter.sourceforge.net\n");
}

static void print_version(void)
{
    (void)fprintf(stderr,
		  "\n"
		  "%s version %s\n"
		  "Copyright (C) 2002 Eric S. Raymond\n\n"
		  "%s comes with ABSOLUTELY NO WARRANTY. "
		  "This is free software, and you\nare welcome to "
		  "redistribute it under the General Public License. "
		  "See the\nCOPYING file with the source distribution for "
		  "details.\n"
		  "\n", 
		  PACKAGE, version, PACKAGE);
}

#ifndef	ENABLE_GRAHAM_METHOD
#define	G ""
#else
#define	G "g"
#endif

#ifndef	ENABLE_ROBINSON_METHOD
#define	R ""
#else
#define	R "r"
#endif

#ifndef	ENABLE_ROBINSON_FISHER
#define	F ""
#else
#define	F "f"
#endif

/** This function processes command line arguments.
 **
 ** It is called twice so that command line arguments
 ** will override config file parameters.
 ** The config file is read in between the two calls to
 ** the function.
 **
 ** The function will exit if there's an error, for example if
 ** there are leftover command line arguments.
 */

void process_args(int argc, char **argv, int pass)
{
    int option;
    int exitcode;
    int errflag = 0;

    test = 0;
    verbose = 0;
    run_type = RUN_UNKNOWN;
    fpin = stdin;
    dbgout = stderr;
    set_today();		/* compute current date for token age */
    select_algorithm(algorithm, false);	/* select default algorithm */

    optind = opterr = 1;
    /* don't use #ifdef here: */
#if HAVE_DECL_OPTRESET
    optreset = 1;
#endif
    while ((option = getopt(argc, argv, ":23d:eFhlL:m:o:snSNvVpuc:CgrRfqtI:O:y:x:DT" G R F)) != -1)
    {
#if 0
	if (getenv("BOGOFILTER_DEBUG_OPTIONS")) {
	    fprintf(stderr, "config: option=%c (%d), optind=%d, opterr=%d, optarg=%s\n", 
		    isprint((unsigned char)option) ? option : '_', option, 
		    optind, opterr, optarg ? optarg : "(null)");
	}
#endif
	switch(option)
	{
	case '2':
	case '3':
	    twostate = option == '2';
	    threestate = option == '3';
	    break;

	case 'd':
	    xfree(directory);
	    directory = xstrdup(optarg);
	    if (pass == 2 && setup_wordlists(directory, PR_COMMAND) != 0)
		exit(2);
	    break;

	case 'e':
	    nonspam_exits_zero = true;
	    break;

	case 's':
	    run_type = (run_type | REG_SPAM) & ~REG_GOOD & ~UNREG_SPAM;
	    break;

	case 'n':
	    run_type = (run_type | REG_GOOD) & ~REG_SPAM & ~UNREG_GOOD;
	    break;

	case 'S':
	    run_type = (run_type | UNREG_SPAM) & ~REG_SPAM & ~UNREG_GOOD;
	    break;

	case 'N':
	    run_type = (run_type | UNREG_GOOD) & ~REG_GOOD & ~UNREG_SPAM;
	    break;

	case 'v':
	    verbose++;
	    break;

	case ':':
	    fprintf(stderr, "Option -%c requires an argument.\n", optopt);
	    exit(2);

	case '?':
	    fprintf(stderr, "Unknown option -%c.\n", optopt);
	    exit(2);

	case 'h':
	    help();
            exit(0);

        case 'V':
	    print_version();
	    exit(0);

	case 'I':
	    if (pass == 2) {
		fpin = fopen( optarg, "r" );
		if (fpin == NULL) {
		    fprintf(stderr, "Can't read file '%s'\n", optarg);
		    exit(2);
		}
	    }
	    break;

        case 'O':
	    xstrlcpy(outfname, optarg, sizeof(outfname));
	    break;

	case 'p':
	    passthrough = true;
	    break;

	case 'u':
	    run_type = RUN_UPDATE;
	    break;

	case 'L':
	    logtag = optarg;
	    /*@fallthrough@*/
	case 'l':
	    logflag = true;
	    break;

#ifdef	GRAHAM_AND_ROBINSON
	case 'g':
	    select_algorithm(AL_GRAHAM, true);
	    break;
#endif

#if	defined(ENABLE_ROBINSON_METHOD) && (defined(ENABLE_GRAHAM_METHOD) || defined(ENABLE_ROBINSON_FISHER))
	case 'r':
	    select_algorithm(AL_ROBINSON, true);
	    break;
#endif

#if	defined(ENABLE_ROBINSON_FISHER) && (defined(ENABLE_GRAHAM_METHOD) || defined(ENABLE_ROBINSON_METHOD))
	case 'f':
	    select_algorithm(AL_FISHER, true);
	    break;
#endif

#ifdef	ROBINSON_OR_FISHER
	case 'R':
	    Rtable = 1;
	    if (algorithm != AL_ROBINSON && algorithm != AL_FISHER)
		if (AL_DEFAULT == AL_ROBINSON || AL_DEFAULT == AL_FISHER)
		    algorithm = AL_DEFAULT;
	    break;
#endif

	case 'x':
	    set_debug_mask( optarg );
	    break;

	case 'q':
	    quiet = true;
	    break;

	case 'F':
	    force = true;
	    break;

	case 'c':
	    /* only read config file on pass #1 so that 
	    ** options can be overridden on pass #2
	    */
	    if (pass == 1 )			
		read_config_file(optarg, false, !quiet, PR_CFG_USER);

	/*@fallthrough@*/
	/* fall through to suppress reading config files */

	case 'C':
	    suppress_config_file = true;
	    break;

	case 'm':
	    comma_parse(option, optarg, &min_dev, &robs);
	    break;

	case 'o':
	    comma_parse(option, optarg, &spam_cutoff, &ham_cutoff);
	    break;

	case 't':
	    terse = true;
	    break;

	case 'y':		/* date as YYYYMMDD */
	    today = string_to_date((char *)optarg);
	    break;

	case 'D':
	    dbgout = stdout;
	    break;

	case 'T':
	    test += 1;
	    break;

	default:
	    /* Mismatch between switch() construct and optstring */
	    print_error(__FILE__, __LINE__, "Internal error: unhandled option '%c' "
			"(%02X)\n", isprint((unsigned char)option) ? option : '_', (unsigned int)option);
	    exit(2);
	}
    }

    if (errflag)
	exit(2);

    if (run_type == RUN_UNKNOWN)
	run_type = RUN_NORMAL;

    exitcode = validate_args();
    if (exitcode) 
	exit (exitcode);

    if (optind < argc) {
	fprintf(stderr, "Extra arguments given, first: %s. Aborting.\n", argv[optind]);
	exit(2);
    }

    return;
}

#define YN(b) (b ? "yes" : "no")

void query_config(void)
{
    fprintf(stdout, "%-11s = %s\n", "algorithm", method->name);
    fprintf(stdout, "%-11s = %0.6f (%8.2e)\n", "robx", robx, robx);
    fprintf(stdout, "%-11s = %0.6f (%8.2e)\n", "robs", robs, robs);
    fprintf(stdout, "%-11s = %0.6f (%8.2e)\n", "min_dev", min_dev, min_dev);
    fprintf(stdout, "%-11s = %0.6f (%8.2e)\n", "ham_cutoff", ham_cutoff, ham_cutoff);
    fprintf(stdout, "%-11s = %0.6f (%8.2e)\n", "spam_cutoff", spam_cutoff, spam_cutoff);
    fprintf(stdout, "\n");
    fprintf(stdout, "%-10s = %s\n", "block_on_subnets", YN(block_on_subnets));
    fprintf(stdout, "%-10s = %s\n", "strict_check", YN(strict_check));
    fprintf(stdout, "%-10s = %s\n", "tag_header_lines", YN(tag_header_lines));
    fprintf(stdout, "%-10s = %s\n", "replace_nonascii_characters", YN(replace_nonascii_characters));
    fprintf(stdout, "\n");
    fprintf(stdout, "%-17s = '%s'\n", "spam_header_name", spam_header_name);
    fprintf(stdout, "%-17s = '%s'\n", "header_format", header_format);
    fprintf(stdout, "%-17s = '%s'\n", "terse_format", terse_format);
    fprintf(stdout, "%-17s = '%s'\n", "log_header_format", log_header_format);
    fprintf(stdout, "%-17s = '%s'\n", "log_update_format", log_update_format);
    display_tag_array("spamicity_tags   ", &spamicity_tags);
    display_tag_array("spamicity_formats", &spamicity_formats);
    exit(0);
}

static void display_tag_array(const char *label, const char **array)
{
    size_t i;
    size_t count = twostate ? 2 : 3;
    const char *s;

    fprintf(stdout, "%s =", label);
    for (i = 0, s = ""; i < count; i += 1, s = ",")
	fprintf(stdout, "%s '%s'", s, array[i]);
    fprintf(stdout, "\n");
}

