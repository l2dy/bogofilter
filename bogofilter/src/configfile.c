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

/*  remove trailing comment from the line.
 */
static char *remove_comment(char *line)
{
    char *tmp = strchr(line, '#');
    if (tmp != NULL) {
	tmp -= 1;
	while (tmp > line && isspace((unsigned char)*tmp))
	    tmp -= 1;
	*(tmp+1) = '\0';
    }
    return line;
}

static bool process_config_parameter(const parm_desc *arg, char *val)
/* returns true if ok, false if error */
{
    bool ok = true;
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
		remove_comment(val);
		if (!xatoi(arg->addr.i, val))
		    return false;
		if (DEBUG_CONFIG(0))
		    fprintf(dbgout, "%s -> %d\n", arg->name, *arg->addr.i);
		break;
	    }
	case CP_DOUBLE:
	    {
		remove_comment(val);
		if (!xatof(arg->addr.d, val))
		    return false;
		if (DEBUG_CONFIG(0))
		    fprintf(dbgout, "%s -> %f\n", arg->name, *arg->addr.d);
		break;
	    }
	case CP_CHAR:
	    {
		*arg->addr.c = *val;
		if (DEBUG_CONFIG(0))
		    fprintf(dbgout, "%s -> '%c'\n", arg->name, *arg->addr.c);
		break;
	    }
	case CP_STRING:
	    {
		*arg->addr.s = xstrdup(val);
		if (DEBUG_CONFIG(0))
		    fprintf(dbgout, "%s -> '%s'\n", arg->name, *arg->addr.s);
		break;
	    }
	case CP_DIRECTORY:
	    {
		char *dir = *arg->addr.s;
		xfree(dir);
		*arg->addr.s = dir = tildeexpand(val);
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
		fprintf(dbgout, "%s -> '%s'\n", arg->name, val);
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

static bool process_config_line(char *line,
				char *val,
				const parm_desc *parms )
/* returns true if parm is processed, false if not */
{
    const parm_desc *arg;

    if (parms == NULL)
	return false;

    for ( arg = parms; arg->name != NULL; arg += 1 )
    {
	if (DEBUG_CONFIG(1))
	    fprintf(dbgout, "Testing:  %s\n", arg->name);
	if (strcmp(arg->name, line) == 0)
	{
	    bool ok = process_config_parameter(arg, val);
	    if (ok && DEBUG_CONFIG(1))
		fprintf(dbgout, "%s\n", "   Found it!");
	    return ok;
	}
    }
    return false;
}

bool read_config_file(const char *fname, bool tilde_expand)
/* returns true if ok, false if error */
{
    bool error = false;
    int lineno = 0;
    FILE *fp;
    char *filename;
    char *arg = NULL, *val = NULL;

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
	return false;
    }

    if (DEBUG_CONFIG(0))
	fprintf(dbgout, "Reading %s\n", filename);

    while (!feof(fp))
    {
	const char delim[] = " \t=";
	size_t len;
	char buff[MAXBUFFLEN];

	memset(buff, '\0', sizeof(buff));		/* for debugging */

	lineno += 1;
	if (fgets(buff, sizeof(buff), fp) == NULL)
	    break;
	len = strlen(buff);
	if ( buff[0] == '#' || buff[0] == ';' || buff[0] == '\n' )
	    continue;
	while (iscntrl((unsigned char)buff[len-1]) || isspace((unsigned char)buff[len-1]))
	    buff[--len] = '\0';

	arg = buff;
	if (strcspn(arg, delim) < strlen(arg)) { /* if delimiter present */
	    val = arg + strcspn(arg, delim);
	    *val++ = '\0';
	    val += strspn(val, delim);
	} else {
	    val = NULL;
	}

	if (val == NULL ||
	       (! process_config_line(arg, val, usr_parms ) &&
		! process_config_line(arg, val, sys_parms ) &&
		! process_config_line(arg, val, format_parms )))
	{
	    error = true;
	    if (!quiet)
		fprintf(stderr, "%s:%d:  Error - bad parameter in '%s'...'%s'\n", filename, lineno, buff, val);
	}
    }

    if (ferror(fp)) {
	fprintf(stderr, "Read error while reading file \"%s\".", filename);
	error = true;
    }

    (void)fclose(fp); /* we're just reading, so fclose should succeed */
    xfree(filename);

    return (error);
}

/* exported */
bool process_config_files(void)
{
    if (!suppress_config_file &&
	(!read_config_file(system_config_file, false) ||
	 !read_config_file(user_config_file, true)))
	 return false;

    stats_prefix= stats_in_header ? "\t" : "#   ";

    if (DEBUG_CONFIG(0))
	fprintf(dbgout, "stats_prefix: '%s'\n", stats_prefix);

    init_charset_table(charset_default, true);

    return true;
}
