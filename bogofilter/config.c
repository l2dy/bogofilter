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

#include "config.h"
#include "common.h"
#include "debug.h"
#include "wordlists.h"
#include "xmalloc.h"
#include "xstrdup.h"

#ifndef	DEBUG_CONFIG
#define DEBUG_CONFIG(level)	(verbose > level)
#endif

/*---------------------------------------------------------------------------*/

// Global variables

extern int verbose;
extern int thresh_index;
extern double thresh_stats;
extern double thresh_rtable;
extern char *spam_header_name;

/*---------------------------------------------------------------------------*/

#define	MAXBUFFLEN	200

typedef enum {
	CP_INTEGER,
	CP_DOUBLE,
	CP_STRING,
	CP_WORDLIST,
} ArgType;

typedef struct {
    const char 	*name;
    ArgType	type;
    union	
    {
	int	*i;
	double	*d;
	char	**s;
    } addr;
} ArgDefinition;

ArgDefinition ArgDefList[] =
{
    { "thresh_index",	CP_INTEGER,	{ (void *) &thresh_index } },
    { "thresh_rtable",	CP_DOUBLE,	{ (void *) &thresh_rtable } },
    { "thresh_stats",	CP_DOUBLE,	{ (void *) &thresh_stats } },
    { "spam_header_name",CP_STRING,	{ (void *) &spam_header_name } },
    { "wordlist",	CP_WORDLIST,	{ (void *) NULL } },
};

int	ArgDefListSize = sizeof(ArgDefList)/sizeof(ArgDefList[0]);

bool process_config_parameter( ArgDefinition * arg, const char *val)
{
    bool ok = TRUE;
    while (isspace(*val) || *val == '=') val += 1;
    switch (arg->type)
    {
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
    case CP_STRING:
    {
	*arg->addr.s = xstrdup( (char *) val );
	if (DEBUG_CONFIG(0))
	    fprintf( stderr, "%s -> '%s'\n", arg->name, *arg->addr.s );
	break;
    }
    case CP_WORDLIST:
    {
	configure_wordlist(val);
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

bool process_config_line( const char *line )
{
    int i;
    int len;
    const char *val;

    for (val=line; *val != '\0'; val += 1)
	if (isspace(*val) || *val == '=' )
	    break;
    len = val - line;
    for (i=0; i<ArgDefListSize; i += 1)
    {
	ArgDefinition *arg = &ArgDefList[i];
	if (strncmp(arg->name, line, len) == 0)
	{
	    bool ok = process_config_parameter(arg, val);
	    return ok;
	}
    }
    return FALSE;
}

void read_config_file(const char *filename)
{
    bool error = FALSE;
    int lineno = 0;
    char *tmp = NULL;
    FILE *fp;

    if ( *filename == '~' )
    {
	char *home = getenv( "HOME" );
	if ( home == NULL )
	{
	    fprintf( stderr, "Can't find $HOME.\n" );
	    exit(2);
	}
	tmp = xmalloc(strlen(home) + strlen(filename) + 2);
	strcpy( tmp, home );
	if (tmp[strlen(tmp)-1] != '/' )
	    strcat( tmp, "/" );
	while (*filename == '~' || *filename == '/')
	    filename += 1;
	strcat( tmp, filename );
	filename = tmp;
    }

    fp = fopen( filename, "r");

    if (fp == NULL)
	return;

    if (DEBUG_CONFIG(0))
	fprintf( stderr, "Reading %s\n", filename );

    while (!feof(fp))
    {
	int len;
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

    fclose(fp);

    if (error)
	exit(2);
}
