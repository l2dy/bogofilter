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
#include "common.h"
#include "debug.h"
#include "wordlists.h"
#include "xmalloc.h"
#include "xstrdup.h"
#include "find_home.h"
#include "globals.h"
#include "bogoconfig.h"
#include "bogofilter.h"

#ifndef	DEBUG_CONFIG
#define DEBUG_CONFIG(level)	(verbose > level)
#endif

/*---------------------------------------------------------------------------*/

/* NOTE: MAXBUFFLEN _MUST_ _NOT_ BE LARGER THAN INT_MAX! */
#define	MAXBUFFLEN	((int)200)

/*---------------------------------------------------------------------------*/

/* Global variables */

int logflag;
int Rtable = 0;

int verbose, passthrough, force, nonspam_exits_zero;
static int suppress_config_file;

char directory[PATH_LEN + 100] = "";
const char *system_config_file = "/etc/bogofilter.cf";
const char *user_config_file   = ".bogofilter.cf";
const char *spam_header_name = SPAM_HEADER_NAME;

bool	stats_in_header = TRUE;
const char *stats_prefix;

run_t run_type = RUN_NORMAL; 
algorithm_t algorithm = AL_GRAHAM;

double	min_dev = 0.0f;
double	robx = 0.0f;
double	robs = 0.0f;

int thresh_index = 0;
double thresh_stats = 0.0f;
double thresh_rtable = 0.0f;

/*---------------------------------------------------------------------------*/

typedef enum {
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
    } addr;
} ArgDefinition;

static int dummy;

static const ArgDefinition ArgDefList[] =
{
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

static const int ArgDefListSize = sizeof(ArgDefList)/sizeof(ArgDefList[0]);

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
	case CP_WORDLIST:
	    {
		configure_wordlist(val); /* FIXME */
		break;
	    }
	default:
	    {
		ok = FALSE;
		fprintf( stderr, "Unknown parameter type for '%s'\n", arg->name );
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

static void read_config_file(const char *filename, bool home_dir)
{
    bool error = FALSE;
    int lineno = 0;
    char *tmp = NULL;
    FILE *fp;

    if (home_dir && filename[0] != '/')
    {
	const char *home = find_home(1);
	if (home == NULL)
	{
	    fprintf(stderr, "Can't find home directory.\n");
	    exit(2);
	}
	if (memcmp(filename, "~/", 2) == 0 )
	    filename += 2;
	tmp = xmalloc(strlen(home) + strlen(filename) + 2);
	strcpy(tmp, home);
	if (tmp[strlen(tmp)-1] != '/')
	    strcat(tmp, "/");
	strcat(tmp, filename);
	filename = tmp;
    }

    fp = fopen(filename, "r");

    if (fp == NULL) {
	if (DEBUG_CONFIG(0)) {
	    fprintf(stderr, "Debug: cannot open %s: %s", filename, strerror(errno));
	}
	xfree(tmp);
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
	    fprintf( stderr, "Unknown config line #%d\n", lineno );
	    fprintf( stderr, "    %s\n", buff );
	    error = TRUE;
	}
    }

    if (ferror(fp)) {
	fprintf(stderr, "Read error while reading file \"%s\".", filename);
	error = TRUE;
    }

    (void)fclose(fp); /* we're just reading, so fclose should succeed */
    xfree(tmp);

    if (error)
	exit(2);
}

static int validate_args(/*@unused@*/ int argc, /*@unused@*/ char **argv)
{
    bool registration, classification;

/*  flags '-s', '-n', '-S', or '-N', are mutually exclusive of flags '-p', '-u', '-e', and '-R'. */
    classification = (run_type == RUN_NORMAL) ||(run_type == RUN_UPDATE) || passthrough || nonspam_exits_zero || (Rtable != 0);
    registration   = (run_type == REG_SPAM) || (run_type == REG_GOOD) || (run_type == REG_GOOD_TO_SPAM) || (run_type == REG_SPAM_TO_GOOD);

    if ( registration && classification)
    {
	(void)fprintf(stderr, "Error:  Invalid combination of options.\n");
	(void)fprintf(stderr, "\n");
	(void)fprintf(stderr, "    Options '-s', '-n', '-S', and '-N' are used when registering words.\n");
	(void)fprintf(stderr, "    Options '-p', '-u', '-e', and '-R' are used when classifying messages.\n");
	(void)fprintf(stderr, "    The two sets of options may not be used together.\n");
	(void)fprintf(stderr, "    \n");
#if 0
	(void)fprintf(stderr, "    Options '-g', '-r', '-l', '-d', '-x', and '-v' may be used with either mode.\n");
#endif
	return 2;
    }

    return 0;
}

int process_args(int argc, char **argv)
{
    int option;
    int exitcode;

    while ((option = getopt(argc, argv, "d:ehlsnSNvVpugc:CRrx:f")) != EOF)
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

	case 'h':
	    (void)printf( "\n" );
	    (void)printf( "Usage: bogofilter [options] < message\n" );
	    (void)printf( "\t-h\t- print this help message and exit.\n" );
	    (void)printf( "\t-d path\t- specify directory for wordlists.\n" );
	    (void)printf( "\t-p\t- passthrough.\n" );
	    (void)printf( "\t-e\t- in -p mode, exit with code 0 when the mail is not spam.\n");
	    (void)printf( "\t-s\t- register message as spam.\n" );
	    (void)printf( "\t-n\t- register message as non-spam.\n" );
	    (void)printf( "\t-S\t- move message's words from non-spam list to spam list.\n" );
	    (void)printf( "\t-N\t- move message's words from spam list to spam non-list.\n" );
	    (void)printf( "\t-v\t- set verbosity level.\n" );
	    (void)printf( "\t-x LIST\t- set debug flags.\n" );
	    (void)printf( "\t-V\t- print version information and exit.\n" );
	    (void)printf( "\t-c filename\t- read config file 'filename'.\n" );
	    (void)printf( "\t-C\t- don't read standard config files.\n" );
	    (void)printf( "\n" );
	    (void)printf( "bogofilter is a tool for classifying email as spam or non-spam.\n" );
	    (void)printf( "\n" );
	    (void)printf( "For updates and additional information, see\n" );
	    (void)printf( "URL: http://bogofilter.sourceforge.net\n" );
	    (void)printf( "\n" );
	    exit(0);

        case 'V':
            (void)printf("\n%s version %s ", PACKAGE, VERSION);
            (void)printf("Copyright (C) 2002 Eric S. Raymond\n\n");
            (void)printf("%s comes with ABSOLUTELY NO WARRANTY. ", PACKAGE);
            (void)printf("This is free software, and you\nare welcome to ");
            (void)printf("redistribute it under the General Public License. ");
            (void)printf("See the\nCOPYING file with the source distribution for ");
            (void)printf("details.\n\n");
            exit(0);

	case 'p':
	    passthrough = 1;
	    break;

	case 'u':
	    run_type = RUN_UPDATE;
	    break;

	case 'l':
	    logflag = 1;
	    break;

	case 'g':
	    algorithm = AL_GRAHAM;
	    break;

	case 'R':
	{
	    Rtable = 1;
	}
	/*@fallthrough@*/
	/* fall through to force Robinson calculations */
	case 'r':
	    algorithm = AL_ROBINSON;
	    break;

	case 'x':
	    set_debug_mask( optarg );
	    break;

	case 'f':
	    force = 1;
	    break;

	case 'c':
	    system_config_file = optarg;
	    read_config_file(system_config_file, FALSE);
	/*@fallthrough@*/
	/* fall through to suppress reading config files */

	case 'C':
	    suppress_config_file = 1;
	    break;
	}
    }

    exitcode = validate_args(argc, argv);

    return exitcode;
}

void process_config_files()
{
    if ( ! suppress_config_file)
    {
	read_config_file(system_config_file, FALSE);
	read_config_file(user_config_file, TRUE);
    }

    stats_prefix= stats_in_header ? "\t" : "#   ";

    if (DEBUG_CONFIG(0))
	fprintf( stderr, "stats_prefix: '%s'\n", stats_prefix );
}
