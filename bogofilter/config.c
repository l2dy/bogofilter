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

#include "config.h"

#include "bogoconfig.h"
#include "bogofilter.h"
#ifdef	HAVE_CHARSET_H
#include "charset.h"
#endif
#include "common.h"
#include "find_home.h"
#include "globals.h"
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
int force;		/* '-f' */
int logflag;		/* '-l' */
int quiet;		/* '-q' */
int passthrough;	/* '-p' */
int verbose;		/* '-v' */
int Rtable = 0;		/* '-R' */

static bool suppress_config_file = FALSE;

char directory[PATH_LEN + 100] = "";
const char *system_config_file = "/etc/bogofilter.cf";
const char *user_config_file   = "~/.bogofilter.cf";
const char *spam_header_name = SPAM_HEADER_NAME;

bool	stats_in_header = TRUE;
const char *stats_prefix;

run_t run_type = RUN_NORMAL; 
algorithm_t algorithm = AL_DEFAULT;

double	min_dev = 0.0f;
double	robx = 0.0f;
double	robs = 0.0f;

int thresh_index = 0;
double thresh_stats = 0.0f;
double thresh_rtable = 0.0f;

/*---------------------------------------------------------------------------*/

typedef enum {
#ifdef	GRAHAM_AND_ROBINSON
	CP_ALGORITHM,
#endif
	CP_BOOLEAN,
	CP_INTEGER,
	CP_DOUBLE,
	CP_CHAR,
	CP_STRING,
	CP_WORDLIST
} ArgType;

typedef struct {
    const char *name;
    ArgType	type;
    union
    {
	bool	*b;
	int	*i;
	double	*d;
	char	*c;
	char	**s;
#ifdef	GRAHAM_AND_ROBINSON
	algorithm_t	*a;
#endif
    } addr;
} ArgDefinition;

static int dummy;

static const ArgDefinition ArgDefList[] =
{
#ifdef	GRAHAM_AND_ROBINSON
    { "algorithm",  	  CP_ALGORITHM,	{ (void *)&algorithm } },
#endif
    { "stats_in_header",  CP_BOOLEAN,	{ (void *)&stats_in_header } },
    { "min_dev",	  CP_DOUBLE,	{ (void *)&min_dev } },
    { "robx",		  CP_DOUBLE,	{ (void *)&robx } },
    { "robs",		  CP_DOUBLE,	{ (void *)&robs } },
    { "thresh_index",	  CP_INTEGER,	{ (void *)&thresh_index } },
    { "thresh_rtable",	  CP_DOUBLE,	{ (void *)&thresh_rtable } },
    { "thresh_stats",	  CP_DOUBLE,	{ (void *)&thresh_stats } },
    { "spam_header_name", CP_STRING,	{ (void *)&spam_header_name } },
    { "user_config_file", CP_STRING,	{ (void *)&user_config_file } },
    { "wordlist",	  CP_WORDLIST,	{ (void *)&dummy } },
};

static const int ArgDefListSize = (int)sizeof(ArgDefList)/sizeof(ArgDefList[0]);

static bool process_config_parameter(const ArgDefinition * arg, const char *val)
{
    bool ok = TRUE;
    while (isspace(*val) || *val == '=') val += 1;
    switch (arg->type)
    {
	case CP_BOOLEAN:
	    {
		char ch=toupper(*val);
		switch (ch)
		{
		case 'Y':		/* Yes */
		case 'T':		/* True */
		case '1':
		    *arg->addr.b = TRUE;
		    break;
		case 'N':		/* No */
		case 'F':		/* False */
		case '0':
		    *arg->addr.b = FALSE;
		    break;
		}
		if (DEBUG_CONFIG(0))
		    fprintf( stderr, "%s -> %s\n", arg->name, *arg->addr.b == TRUE ? "Yes" : "No" );
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
#ifdef	GRAHAM_AND_ROBINSON
	case CP_ALGORITHM:
	{
	    char ch = tolower(*val);
	    switch (ch)
	    {
		case 'g':
		*arg->addr.a = AL_GRAHAM;
	    break;
	    case 'r':
		*arg->addr.a = AL_ROBINSON;
		break;
	    default:
		ok = FALSE;
		break;
	    }
	    if (DEBUG_CONFIG(0))
		fprintf( stderr, "%s -> '%c'\n", arg->name, *arg->addr.a );
	    break;
	}
#endif
	case CP_WORDLIST:
	    {
		if (!configure_wordlist(val))
		    ok = FALSE;
		break;
	    }
	default:
	    {
		ok = FALSE;
		break;
	    }
    }
    return ok;
}

static bool process_config_line( const char *line )
{
    int i;
    size_t len;
    const char *val;

    for (val=line; *val != '\0'; val += 1) {
	if (isspace((unsigned char)*val) || *val == '=') {
	    break;
	}
    }
    len = val - line;
    for (i=0; i<ArgDefListSize; i += 1) {
	const ArgDefinition *arg = &ArgDefList[i];
	if (strncmp(arg->name, line, len) == 0)
	{
	    bool ok = process_config_parameter(arg, val);
	    return ok;
	}
    }
    return FALSE;
}

static void read_config_file(const char *fname, bool tilde_expand)
{
    bool error = FALSE;
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
	    fprintf(stderr, "Debug: cannot open %s: %s", filename, strerror(errno));
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
	if ( ! process_config_line( buff ))
	{
	    error = TRUE;
	    if (!quiet)
		fprintf( stderr, "%s:%d:  Error - unknown parameter in '%s'\n", filename, lineno, buff );
	}
    }

    if (ferror(fp)) {
	fprintf(stderr, "Read error while reading file \"%s\".", filename);
	error = TRUE;
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
#if 0
#ifdef	GRAHAM_AND_ROBINSON
	(void)fprintf(stderr, "    Options '-g', '-r', '-l', '-d', '-x', and '-v' may be used with either mode.\n");
#else
	(void)fprintf(stderr, "    Options '-l', '-d', '-x', and '-v' may be used with either mode.\n");
#endif
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

int process_args(int argc, char **argv)
{
    int option;
    int exitcode;

#ifndef	GRAHAM_AND_ROBINSON
#define	OPTIONS "d:ehlsnSNvVpuc:CRx:fq"
#else
#define	OPTIONS "d:ehlsnSNvVpuc:CgrRx:fq"
#endif

    while ((option = getopt(argc, argv, OPTIONS)) != EOF)
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
	case 'R':
	    Rtable = 1;
#ifdef	GRAHAM_AND_ROBINSON
	/*@fallthrough@*/
	/* fall through to force Robinson calculations */
	case 'r':
	    algorithm = AL_ROBINSON;
#endif
	    break;

	case 'x':
	    set_debug_mask( optarg );
	    break;

	case 'q':
	    quiet = 1;
	    break;

	case 'f':
	    force = 1;
	    break;

	case 'c':
	    read_config_file(optarg, FALSE);
	/*@fallthrough@*/
	/* fall through to suppress reading config files */

	case 'C':
	    suppress_config_file = TRUE;
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
    if (! suppress_config_file)
    {
	read_config_file(system_config_file, FALSE);
	read_config_file(user_config_file, TRUE);
    }

    stats_prefix= stats_in_header ? "\t" : "#   ";

    if (DEBUG_CONFIG(0))
	fprintf( stderr, "stats_prefix: '%s'\n", stats_prefix );

#ifdef	HAVE_CHARSET_H
    init_charset_table("us-ascii", TRUE);
#endif
}
