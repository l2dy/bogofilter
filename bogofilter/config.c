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

int verbose, passthrough, force, nonspam_exits_zero;

char directory[PATH_LEN+73];
const char *user_config_file   = "~/.bogofilter.cf";
const char *spam_header_name = SPAM_HEADER_NAME;
const char *stats_prefix;

run_t run_type = RUN_NORMAL; 

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
		char ch=tolower(*val);
		switch (ch)
		{
		case 'Y':		// Yes
		case 'T':		// True
		case '1':
		    *arg->addr.b = TRUE;
		    break;
		case 'N':		// No
		case 'F':		// False
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
		*arg->addr.c = * (char *)val;
		if (DEBUG_CONFIG(0))
		    fprintf( stderr, "%s -> '%c'\n", arg->name, *arg->addr.c );
		break;
	    }
	case CP_STRING:
	    {
		*arg->addr.s = xstrdup( (char *) val );
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

    for (val=line; *val != '\0'; val += 1)
	if (isspace(*val) || *val == '=' )
	    break;
    len = val - line;
    for (i=0; i<ArgDefListSize; i += 1)
    {
	const ArgDefinition *arg = &ArgDefList[i];
	if (strncmp(arg->name, line, len) == 0)
	{
	    bool ok = process_config_parameter(arg, val);
	    return ok;
	}
    }
    return FALSE;
}

void read_config_file(const char *filename, bool home_dir)
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
