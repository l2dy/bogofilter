/* $Id$ */

/*****************************************************************************

NAME:
   configfile.c -- process config file parameters

   2003-02-12 - split out from config.c so bogolexer use the code.

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
#include "xatox.h"
#include "xmalloc.h"
#include "xstrdup.h"
#include "xstrlcpy.h"

#ifndef	DEBUG_CONFIG
#define DEBUG_CONFIG(level)	(verbose > level)
#endif

/*---------------------------------------------------------------------------*/

/* NOTE: MAXBUFFLEN _MUST_ _NOT_ BE LARGER THAN INT_MAX! */
#define	MAXBUFFLEN	((int)200)

/*---------------------------------------------------------------------------*/

/* Global variables */

const char *user_config_file   = "~/.bogofilter.cf";

bool	stats_in_header = true;

/*---------------------------------------------------------------------------*/

const parm_desc *usr_parms = NULL;

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
		*arg->addr.b = str_to_bool((const char *)val);
		if (DEBUG_CONFIG(0))
		    fprintf(dbgout, "%s -> %s\n", arg->name,
			    *arg->addr.b ? "Yes" : "No");
		break;
	    }
	case CP_INTEGER:
	    {
		if (!xatoi(arg->addr.i, val))
		    fprintf(stderr, "cannot parse integer value '%s'\n", val);
		if (DEBUG_CONFIG(0))
		    fprintf(dbgout, "%s -> %d\n", arg->name, *arg->addr.i);
		break;
	    }
	case CP_DOUBLE:
	    {
		if (!xatof(arg->addr.d, (const char *)val))
		    fprintf(stderr, "cannot parse double value '%s'\n", val);
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
		*arg->addr.s = dir = tildeexpand((const char *)val);
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

void read_config_file(const char *fname, bool fail_on_error, bool tilde_expand)
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
	     ! process_config_line( buff, format_parms ) &&
	     fail_on_error)
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

/* exported */
void process_config_files(bool fail_on_error)
{
    if (! suppress_config_file)
    {
	read_config_file(system_config_file, fail_on_error, false);
	read_config_file(user_config_file, fail_on_error, true);
    }

    stats_prefix= stats_in_header ? "\t" : "#   ";

    if (DEBUG_CONFIG(0))
	fprintf(dbgout, "stats_prefix: '%s'\n", stats_prefix);

    init_charset_table(charset_default, true);

    return;
}
