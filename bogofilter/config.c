/* $Id$ */

/*****************************************************************************

NAME:
   config.c -- process config file parameters

AUTHOR:
   David Relson <relson@osagesoftware.com>

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
#include "maint.h"
#include "error.h"
#include "find_home.h"
#include "format.h"
#include "lexer.h"
#include "maint.h"
#include "wordlists.h"
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

/* NOTE: MAXBUFFLEN _MUST_ _NOT_ BE LARGER THAN INT_MAX! */
#define	MAXBUFFLEN	((int)200)

/*---------------------------------------------------------------------------*/

/* Global variables */

char outfname[PATH_LEN] = "";

const char *user_config_file   = "~/.bogofilter.cf";

bool	stats_in_header = true;

run_t run_type = RUN_NORMAL; 

const char *logtag = NULL;

enum algorithm_e {
#ifdef ENABLE_GRAHAM_METHOD
    AL_GRAHAM='g',
#endif
#ifdef ENABLE_ROBINSON_METHOD
    AL_ROBINSON='r',
#endif
#ifdef ENABLE_ROBINSON_FISHER
    AL_FISHER='f'
#endif
};

/* define default */
#if defined (ENABLE_ROBINSON_METHOD)
#define AL_DEFAULT AL_ROBINSON
#elif defined (ENABLE_GRAHAM_METHOD)
#define AL_DEFAULT AL_GRAHAM
#elif defined (ENABLE_ROBINSON_FISHER)
#define AL_DEFAULT AL_FISHER
#else
#error No algorithms compiled in. See configure --help.
#endif

/* Local variables and declarations */

static bool suppress_config_file = false;
static enum algorithm_e algorithm = AL_DEFAULT;
static bool cmd_algorithm = false;		/* true if specified on command line */

static bool select_algorithm(const unsigned char ch, bool cmdline);
static void config_algorithm(const unsigned char *s);

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

static const parm_desc sys_parms[] =
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
    { "kill_html_comments", 		CP_BOOLEAN, { (void *) &kill_html_comments } },
    { "count_html_comments",  		CP_INTEGER, { (void *) &count_html_comments } },
    { "score_html_comments",  		CP_BOOLEAN, { (void *) &score_html_comments } },
    { NULL,		  CP_NONE,	{ (void *) NULL } },
};

static const parm_desc *usr_parms = NULL;

static void config_algorithm(const unsigned char *s)
{
    select_algorithm(tolower(*s), false);
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

static bool process_config_parameter(const parm_desc *arg, const unsigned char *val)
{
    bool ok = true;
    while (isspace(*val) || *val == '=') val += 1;
    if (arg->addr.v == NULL)
	return ok;
    switch (arg->type)
    {
	case CP_BOOLEAN:
	    {
		*arg->addr.b = str_to_bool(val);
		if (DEBUG_CONFIG(0))
		    fprintf(dbgout, "%s -> %s\n", arg->name,
			    *arg->addr.b ? "Yes" : "No");
		break;
	    }
	case CP_INTEGER:
	    {
		int sign = (*val == '-') ? -1 : 1;
		if (*val == '-' || *val == '+')
		    val += 1;
		*arg->addr.i = atoi((const char *)val) * sign;
		if (DEBUG_CONFIG(0))
		    fprintf(dbgout, "%s -> %d\n", arg->name, *arg->addr.i);
		break;
	    }
	case CP_DOUBLE:
	    {
		double sign = (*val == '-') ? -1.0f : 1.0f;
		if (*val == '-' || *val == '+')
		    val += 1;
		*arg->addr.d = atof((const char *)val) * sign;
		if (DEBUG_CONFIG(0))
		    fprintf(dbgout, "%s -> %f\n", arg->name, *arg->addr.d);
		break;
	    }
	case CP_CHAR:
	    {
		*arg->addr.c = *(const char *)val;
		if (DEBUG_CONFIG(0))
		    fprintf(dbgout, "%s -> '%c'\n", arg->name, *arg->addr.c);
		break;
	    }
	case CP_STRING:
	    {
		*arg->addr.s = xstrdup((const char *)val);
		if (DEBUG_CONFIG(0))
		    fprintf(dbgout, "%s -> '%s'\n", arg->name, *arg->addr.s);
		break;
	    }
	case CP_DIRECTORY:
	    {
		char *dir = *arg->addr.s;
		xfree(dir);
		*arg->addr.s = dir = xstrdup((const char *)val);
		if (DEBUG_CONFIG(0))
		    fprintf(dbgout, "%s -> '%s'\n", arg->name, dir);
		if (setup_wordlists(dir) != 0)
		    exit(2);
		break;
	    }
	case CP_FUNCTION:
	{
	    ok = (*arg->addr.f)(val);
	    if (DEBUG_CONFIG(0))
		fprintf(dbgout, "%s -> '%c'\n", arg->name, *val);
	    break;
	}
	default:
	    {
		ok = false;
		break;
	    }
    }
    return ok;
}

static bool process_config_line( const unsigned char *line, const parm_desc *parms )
{
    size_t len;
    const unsigned char *val;
    const parm_desc *arg;

    if (parms == NULL)
	return false;

    for (val=line; *val != '\0'; val += 1) {
	if (isspace(*val) || *val == '=') {
	    break;
	}
    }
    len = val - line;
    for ( arg = parms; arg->name != NULL; arg += 1 )
    {
	if (DEBUG_CONFIG(1))
	    fprintf(dbgout, "Testing:  %s\n", arg->name);
	if (strncmp(arg->name, (const char *)line, len) == 0)
	{
	    bool ok = process_config_parameter(arg, val);
	    if (DEBUG_CONFIG(1) && ok )
		fprintf(dbgout, "%s\n", "   Found it!");
	    return ok;
	}
    }
    return false;
}

static void read_config_file(const char *fname, bool tilde_expand)
{
    bool error = false;
    int lineno = 0;
    FILE *fp;
    char *filename;

    if (tilde_expand) {
	filename = tildeexpand(fname);
    } else {
	filename = xstrdup(fname);
    }

    fp = fopen(filename, "r");

    if (fp == NULL) {
	if (DEBUG_CONFIG(0)) {
	    fprintf(dbgout, "Debug: cannot open %s: %s\n", filename, strerror(errno));
	}
	xfree(filename);
	return;
    }

    if (DEBUG_CONFIG(0))
	fprintf(dbgout, "Reading %s\n", filename);

    while (!feof(fp))
    {
	size_t len;
	unsigned char buff[MAXBUFFLEN];

	memset(buff, '\0', sizeof(buff));		/* for debugging */

	lineno += 1;
	if (fgets((char *)buff, sizeof(buff), fp) == NULL)
	    break;
	len = strlen((char *)buff);
	if ( buff[0] == '#' || buff[0] == ';' || buff[0] == '\n' )
	    continue;
	while (iscntrl(buff[len-1]))
	    buff[--len] = '\0';

	if ( ! process_config_line( buff, usr_parms ) &&
	     ! process_config_line( buff, sys_parms ) &&
	     ! process_config_line( buff, format_parms ))
	{
	    error = true;
	    if (!quiet)
		fprintf( stderr, "%s:%d:  Error - unknown parameter in '%s'\n", filename, lineno, buff );
	}
    }

    if (ferror(fp)) {
	fprintf(stderr, "Read error while reading file \"%s\".", filename);
	error = true;
    }

    (void)fclose(fp); /* we're just reading, so fclose should succeed */
    xfree(filename);

    if (error)
	exit(2);
}

static int validate_args(void)
{
    bool registration, classification;

/*  flags '-s', '-n', '-S', or '-N', are mutually exclusive of flags '-p', '-u', '-e', and '-R'. */
    classification = (run_type == RUN_NORMAL) ||(run_type == RUN_UPDATE) || passthrough || nonspam_exits_zero || (Rtable != 0);
    registration   = (run_type == REG_SPAM) || (run_type == REG_GOOD) || (run_type == REG_GOOD_TO_SPAM) || (run_type == REG_SPAM_TO_GOOD);

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
		  "\n"
		  "Usage: %s [options] < message\n"
		  "\t-h\t- print this help message.\n"
		  "\t-d path\t- specify directory for wordlists.\n"
#ifdef	GRAHAM_AND_ROBINSON
#ifdef	ENABLE_GRAHAM_METHOD
		  "\t-g\t- select Graham spam calculation method.\n"
#endif
#ifdef	ENABLE_ROBINSON_METHOD
		  "\t-r\t- select Robinson spam calculation method (default).\n"
#endif
#ifdef	ENABLE_ROBINSON_FISHER
		  "\t-f\t- select Fisher spam calculation method.\n"
#endif
#endif
		  "\t-p\t- passthrough.\n"
		  "\t-I file\t- read message from 'file' instead of stdin.\n"
		  "\t-O file\t- save message to 'file' in passthrough mode.\n"
		  "\t-e\t- in -p mode, exit with code 0 when the mail is not spam.\n"
		  "\t-s\t- register message as spam.\n"
		  "\t-n\t- register message as non-spam.\n"
		  "\t-o val\t- set user defined spamicity cutoff.\n"
		  "\t-u\t- classify message as spam or non-spam and register appropriately.\n"
		  "\t-S\t- move message's words from non-spam list to spam list.\n"
		  "\t-N\t- move message's words from spam list to spam non-list.\n"
		  "\t-R\t- print an R data frame.\n"
		  "\t-x list\t- set debug flags.\n"
		  "\t-v\t- set debug verbosity level.\n"
		  "\t-k y/n\t- kill html comments (yes or no).\n"
		  "\t-V\t- print version information and exit.\n"
		  "\t-c file\t- read specified config file.\n"
		  "\t-C\t- don't read standard config files.\n"
		  "\t-q\t- quiet - don't print warning messages.\n"
		  "\t-F\t- force printing of spamicity numbers.\n"
		  "\n"
		  "bogofilter is a tool for classifying email as spam or non-spam.\n"
		  "\n"
		  "For updates and additional information, see\n"
		  "URL: http://bogofilter.sourceforge.net\n"
		  "\n", 
		  PACKAGE );
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

int process_args(int argc, char **argv)
{
    int option;
    int exitcode;

    set_today();		/* compute current date for token age */

    select_algorithm(algorithm, false);	/* select default algorithm */

    fpin = stdin;

    while ((option = getopt(argc, argv, "d:eFhl::o:snSNvVpuc:CgrRx:fqtI:O:y:k:" G R F)) != EOF)
    {
	switch(option)
	{
	case 'd':
	    xfree(directory);
	    directory = xstrdup(optarg);
	    if (setup_wordlists(directory) != 0)
		exit(2);
	    break;

	case 'e':
	    nonspam_exits_zero = true;
	    break;

	case 's':
	    run_type = REG_SPAM;
	    break;

	case 'n':
	    run_type = REG_GOOD;
	    break;

	case 'S':
	    run_type = REG_GOOD_TO_SPAM;
	    break;

	case 'N':
	    run_type = REG_SPAM_TO_GOOD;
	    break;

	case 'v':
	    verbose++;
	    break;

	case '?':
	    help();
	    exit(2);
	    break;

	case 'h':
	    help();
            exit(0);
	    break;

        case 'V':
	    print_version();
	    exit(0);
	    break;

	case 'I':
	    fpin = fopen( optarg, "r" );
	    if (fpin == NULL) {
		fprintf(stderr, "Can't read file '%s'\n", optarg);
		exit(2);
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

	case 'k':
	    kill_html_comments = str_to_bool( optarg );
	    break;

	case 'l':
	    logflag = true;
	    if (optarg)
		logtag = optarg;
	    break;

#ifdef	GRAHAM_AND_ROBINSON
	case 'g':
	    select_algorithm(AL_GRAHAM, true);
	    break;
#endif

#ifdef	ENABLE_GRAHAM_METHOD
	case 'r':
	    select_algorithm(AL_ROBINSON, true);
	    break;
#endif

#ifdef ENABLE_ROBINSON_FISHER
	case 'f':
	    select_algorithm(AL_FISHER, true);
	    break;
#endif

#ifdef	GRAHAM_OR_ROBINSON
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
	    read_config_file(optarg, false);
	/*@fallthrough@*/
	/* fall through to suppress reading config files */

	case 'C':
	    suppress_config_file = true;
	    break;

	case 'o':
	    spam_cutoff = atof( optarg );
	    break;

	case 't':
	    terse = true;
	    break;

	case 'y':		/* date as YYYYMMDD */
	    today = string_to_date((char *)optarg);
	    break;

	default:
	    print_error(__FILE__, __LINE__, "Internal error: unhandled option '%c' "
			"(%02X)\n", option, (unsigned int)option);
	    exit(2);
	}
    }

    exitcode = validate_args();

    return exitcode;
}

/* exported */
void process_config_files(void)
{
    if (! suppress_config_file)
    {
	read_config_file(system_config_file, false);
	read_config_file(user_config_file, true);
    }

    stats_prefix= stats_in_header ? "\t" : "#   ";

    if (DEBUG_CONFIG(0))
	fprintf(dbgout, "stats_prefix: '%s'\n", stats_prefix);

    init_charset_table(charset_default, true);

    return;
}
