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
#include <unistd.h>

#include <config.h>
#include "common.h"

#include "bogoconfig.h"
#include "bogofilter.h"
#ifdef	HAVE_CHARSET
#include "charset.h"
#endif
#include "directories.h"
#include "find_home.h"
#include "globals.h"
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
#include "wordlists.h"
#include "xmalloc.h"
#include "xstrdup.h"

#ifndef	DEBUG_CONFIG
#define DEBUG_CONFIG(level)	(verbose > level)
#endif

/*---------------------------------------------------------------------------*/

/* NOTE: MAXBUFFLEN _MUST_ _NOT_ BE LARGER THAN INT_MAX! */
#define	MAXBUFFLEN	((int)200)

/*---------------------------------------------------------------------------*/

/* Global variables */

int nonspam_exits_zero;	/* '-e' */
bool force;		/* '-F' */
bool logflag;		/* '-l' */
bool quiet;		/* '-q' */
bool terse;		/* '-t' */
int passthrough;	/* '-p' */
int verbose;		/* '-v' */
int Rtable = 0;		/* '-R' */

static bool suppress_config_file = false;

char directory[PATH_LEN + 100] = "";
const char *system_config_file = SYSCONFDIR "/bogofilter.cf";
const char *user_config_file   = "~/.bogofilter.cf";
const char *spam_header_name = SPAM_HEADER_NAME;

bool	stats_in_header = true;
const	char *stats_prefix;

run_t run_type = RUN_NORMAL; 
method_t *method = NULL;

enum algorithm_e {
#ifdef ENABLE_GRAHAM_METHOD
    AL_GRAHAM='g',
#endif
#ifdef ENABLE_ROBINSON_METHOD
    AL_ROBINSON='r',
#endif
#ifdef ENABLE_ROBINSON_FISHER
    AL_FISHER='f',
#endif
};

/* define default */
#ifdef ENABLE_GRAHAM_METHOD
#define AL_DEFAULT AL_GRAHAM
#else
#define AL_DEFAULT AL_ROBINSON
#endif

static enum algorithm_e algorithm = AL_DEFAULT;

double	spam_cutoff;
double	min_dev = 0.0f;

double	thresh_stats = 0.0f;

/*---------------------------------------------------------------------------*/

static const parm_desc sys_parms[] =
{
    { "stats_in_header",  CP_BOOLEAN,	{ (void *) &stats_in_header } },
    { "spam_header_name", CP_STRING,	{ (void *) &spam_header_name } },
    { "user_config_file", CP_STRING,	{ (void *) &user_config_file } },
    { "wordlist",	  CP_WORDLIST,	{ (void *) NULL } },

    { "algorithm",  	  CP_ALGORITHM,	{ (void *) NULL } },

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
    { NULL,		  CP_NONE,	{ (void *) NULL } },
};

static const parm_desc *usr_parms = NULL;

static bool select_method(enum algorithm_e al)
{
    bool ok = true;
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
	ok = false;
	break;
    }
    usr_parms = method->config_parms;
    return ok;
}

static bool process_config_parameter(const parm_desc *arg, const char *val)
{
    bool ok = true;
    while (isspace(*val) || *val == '=') val += 1;
    if ( arg->addr.v == NULL )
	return ok;
    switch (arg->type)
    {
	case CP_BOOLEAN:
	    {
		char ch = toupper(*val);
		switch (ch)
		{
		case 'Y':		/* Yes */
		case 'T':		/* True */
		case '1':
		    *arg->addr.b = true;
		    break;
		case 'N':		/* No */
		case 'F':		/* False */
		case '0':
		    *arg->addr.b = false;
		    break;
		}
		if (DEBUG_CONFIG(0))
		    fprintf(stderr, "%s -> %s\n", arg->name,
			    *arg->addr.b ? "Yes" : "No");
		break;
	    }
	case CP_INTEGER:
	    {
		int sign = (*val == '-') ? -1 : 1;
		if (*val == '-' || *val == '+')
		    val += 1;
		*arg->addr.i = atoi(val) * sign;
		if (DEBUG_CONFIG(0))
		    fprintf( stderr, "%s -> %d\n", arg->name, *arg->addr.i );
		break;
	    }
	case CP_DOUBLE:
	    {
		double sign = (*val == '-') ? -1.0f : 1.0f;
		if (*val == '-' || *val == '+')
		    val += 1;
		*arg->addr.d = atof(val) * sign;
		if (DEBUG_CONFIG(0))
		    fprintf( stderr, "%s -> %f\n", arg->name, *arg->addr.d );
		break;
	    }
	case CP_CHAR:
	    {
		*arg->addr.c = *(const char *)val;
		if (DEBUG_CONFIG(0))
		    fprintf( stderr, "%s -> '%c'\n", arg->name, *arg->addr.c );
		break;
	    }
	case CP_STRING:
	    {
		*arg->addr.s = xstrdup((const char *)val);
		if (DEBUG_CONFIG(0))
		    fprintf( stderr, "%s -> '%s'\n", arg->name, *arg->addr.s );
		break;
	    }
	case CP_ALGORITHM:
	{
	    ok = select_method( tolower(*val) );
	    if (DEBUG_CONFIG(0))
		fprintf( stderr, "%s -> '%c'\n", arg->name, *val );
	    break;
	}
	case CP_WORDLIST:
	    {
		if (!configure_wordlist(val))
		    ok = false;
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

static bool process_config_line( const char *line, const parm_desc *parms )
{
    size_t len;
    const char *val;
    const parm_desc *arg;

    if (parms == NULL)
	return false;

    for (val=line; *val != '\0'; val += 1) {
	if (isspace((unsigned char)*val) || *val == '=') {
	    break;
	}
    }
    len = val - line;
    for ( arg = parms; arg->name != NULL; arg += 1 )
    {
	if (strncmp(arg->name, line, len) == 0)
	{
	    bool ok = process_config_parameter(arg, val);
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
	    fprintf(stderr, "Debug: cannot open %s: %s\n", filename, strerror(errno));
	}
	xfree(filename);
	return;
    }

    if (DEBUG_CONFIG(0))
	fprintf(stderr, "Reading %s\n", filename);

    while (!feof(fp))
    {
	size_t len;
	char buff[MAXBUFFLEN];

	lineno += 1;
	if (fgets(buff, sizeof(buff), fp) == NULL)
	    break;
	len = strlen(buff);
	if ( buff[0] == '#' || buff[0] == ';' || buff[0] == '\n' )
	    continue;
	while (iscntrl(buff[len-1]))
	    buff[--len] = '\0';

	if ( ! process_config_line( buff, usr_parms ) &&
	     ! process_config_line( buff, sys_parms ))
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

    if (registration && classification)
    {
	(void)fprintf(stderr, "Error:  Invalid combination of options.\n");
	(void)fprintf(stderr, "\n");
	(void)fprintf(stderr, "    Options '-s', '-n', '-S', and '-N' are used when registering words.\n");
	(void)fprintf(stderr, "    Options '-p', '-u', '-e', and '-R' are used when classifying messages.\n");
	(void)fprintf(stderr, "    The two sets of options may not be used together.\n");
	(void)fprintf(stderr, "    \n");
#ifdef	GRAHAM_AND_ROBINSON
	(void)fprintf(stderr, "    Options '-g', '-r', '-l', '-d', '-x', and '-v' may be used with either mode.\n");
#else
	(void)fprintf(stderr, "    Options '-l', '-d', '-x', and '-v' may be used with either mode.\n");
#endif
	return 2;
    }

    return 0;
}

static void help(void)
{
    (void)printf( "\n" );
    (void)printf( "Usage: bogofilter [options] < message\n" );
    (void)printf( "\t-h\t- print this help message.\n" );
    (void)printf( "\t-d path\t- specify directory for wordlists.\n" );
#ifdef	GRAHAM_AND_ROBINSON
    (void)printf( "\t-g\t- select Graham spam calulation method (default).\n" );
    (void)printf( "\t-r\t- select Robinson spam calulation method.\n" );
#endif
    (void)printf( "\t-p\t- passthrough.\n" );
    (void)printf( "\t-e\t- in -p mode, exit with code 0 when the mail is not spam.\n");
    (void)printf( "\t-s\t- register message as spam.\n" );
    (void)printf( "\t-n\t- register message as non-spam.\n" );
    (void)printf( "\t-u\t- classify message as spam or non-spam and register appropriately.\n" );
    (void)printf( "\t-S\t- move message's words from non-spam list to spam list.\n" );
    (void)printf( "\t-N\t- move message's words from spam list to spam non-list.\n" );
    (void)printf( "\t-R\t- print an R data frame.\n" );
    (void)printf( "\t-v\t- set debug verbosity level.\n" );
    (void)printf( "\t-x LIST\t- set debug flags.\n" );
    (void)printf( "\t-V\t- print version information and exit.\n" );
    (void)printf( "\t-c filename\t- read config file 'filename'.\n" );
    (void)printf( "\t-C\t- don't read standard config files.\n" );
    (void)printf( "\t-q\t- quiet - don't print warning messages.\n" );
    (void)printf( "\t-f\t- force printing of spamicity numbers.\n" );
    (void)printf( "\n" );
    (void)printf( "bogofilter is a tool for classifying email as spam or non-spam.\n" );
    (void)printf( "\n" );
    (void)printf( "For updates and additional information, see\n" );
    (void)printf( "URL: http://bogofilter.sourceforge.net\n" );
    (void)printf( "\n" );
}

static void version(void)
{
    (void)printf("\n%s version %s ", PACKAGE, VERSION);
    (void)printf("Copyright (C) 2002 Eric S. Raymond\n\n");
    (void)printf("%s comes with ABSOLUTELY NO WARRANTY. ", PACKAGE);
    (void)printf("This is free software, and you\nare welcome to ");
    (void)printf("redistribute it under the General Public License. ");
    (void)printf("See the\nCOPYING file with the source distribution for ");
    (void)printf("details.\n\n");
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

    while ((option = getopt(argc, argv, "d:ehlsnSNvVpuc:CgrRx:fqt" G R F)) != EOF)
    {
	switch(option)
	{
	case 'd':
	    strncpy(directory, optarg, PATH_LEN);
	    break;

	case 'e':
	    nonspam_exits_zero = 1;
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
	case 'h':
	    help();
            exit(0);
	    break;

        case 'V':
	    version();
	    exit(0);
	    break;

	case 'p':
	    passthrough = 1;
	    break;

	case 'u':
	    run_type = RUN_UPDATE;
	    break;

	case 'l':
	    logflag = 1;
	    break;

#ifdef	GRAHAM_AND_ROBINSON
	case 'g':
	    algorithm = AL_GRAHAM;
	    break;
#endif

#ifdef	ENABLE_ROBINSON_METHOD
	case 'R':
	    Rtable = 1;
#ifdef	ENABLE_GRAHAM_METHOD
	/*@fallthrough@*/
	/* fall through to force Robinson calculations */
	case 'r':
	    algorithm = AL_ROBINSON;
#endif
	    break;
#endif

#ifdef ENABLE_ROBINSON_FISHER
	case 'f':
	    algorithm = AL_FISHER;
	    break;
#endif
	case 'x':
	    set_debug_mask( optarg );
	    break;

	case 'q':
	    quiet = 1;
	    break;

	case 'F':
	    force = 1;
	    break;


	case 'c':
	    read_config_file(optarg, false);
	/*@fallthrough@*/
	/* fall through to suppress reading config files */

	case 'C':
	    suppress_config_file = true;
	    break;

	case 't':
	    terse = true;
	    break;

	default:
	    fprintf( stderr, "Unknown option '%c' (%02X)\n", option,
		    (unsigned int)option);
	    exit(2);
	}
    }

    exitcode = validate_args();

    return exitcode;
}

/* exported */
void process_config_files(void)
{
    select_method(algorithm);

    if (! suppress_config_file)
    {
	read_config_file(system_config_file, false);
	read_config_file(user_config_file, true);
    }

    stats_prefix= stats_in_header ? "\t" : "#   ";

    if (DEBUG_CONFIG(0))
	fprintf( stderr, "stats_prefix: '%s'\n", stats_prefix );

#ifdef	HAVE_CHARSET
    init_charset_table("us-ascii", true);
#endif

    return;
}
