/* $Id$ */

/*****************************************************************************

NAME:
   configfile.c -- process config file parameters

   2003-02-12 - split out from config.c so bogolexer use the code.

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#include "common.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "bogofilter.h"
#include "bool.h"
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

#ifndef	DEBUG_CONFIG
#define DEBUG_CONFIG(level)	(verbose > level)
#endif

/*---------------------------------------------------------------------------*/

/* NOTE: MAXBUFFLEN _MUST_ _NOT_ BE LARGER THAN INT_MAX! */
#define	MAXBUFFLEN	((int)200)

/*---------------------------------------------------------------------------*/

/* Global variables */

#ifndef __riscos__
const char *user_config_file   = "~/.bogofilter.cf";
#else
const char *user_config_file   = "Choices:bogofilter.bogofilter/cf";
#endif

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

static bool process_config_parameter(const parm_desc *arg, char *val, priority_t precedence)
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
		if (DEBUG_CONFIG(2))
		    fprintf(dbgout, "%s -> %s\n", arg->name,
			    *arg->addr.b ? "Yes" : "No");
		break;
	    }
	case CP_INTEGER:
	    {
		remove_comment(val);
		if (!xatoi(arg->addr.i, val))
		    return false;
		if (DEBUG_CONFIG(2))
		    fprintf(dbgout, "%s -> %d\n", arg->name, *arg->addr.i);
		break;
	    }
	case CP_DOUBLE:
	    {
		remove_comment(val);
		if (!xatof(arg->addr.d, val))
		    return false;
		if (DEBUG_CONFIG(2))
		    fprintf(dbgout, "%s -> %f\n", arg->name, *arg->addr.d);
		break;
	    }
	case CP_CHAR:
	    {
		*arg->addr.c = *val;
		if (DEBUG_CONFIG(2))
		    fprintf(dbgout, "%s -> '%c'\n", arg->name, *arg->addr.c);
		break;
	    }
	case CP_STRING:
	    {
		*arg->addr.s = xstrdup(val);
		if (DEBUG_CONFIG(2))
		    fprintf(dbgout, "%s -> '%s'\n", arg->name, *arg->addr.s);
		break;
	    }
	case CP_DIRECTORY:
	    {
		char *dir = tildeexpand(val, true);
		if (DEBUG_CONFIG(2))
		    fprintf(dbgout, "%s -> '%s'\n", arg->name, dir);
		if (setup_wordlists(dir, precedence) != 0)
		    exit(EX_ERROR);
		xfree(dir);
		break;
	    }
	case CP_FUNCTION:
	{
	    ok = (*arg->addr.f)((unsigned char *)val);
	    if (DEBUG_CONFIG(2))
		fprintf(dbgout, "%s -> '%s'\n", arg->name, val);
	    break;
	}
#ifdef	ENABLE_DEPRECATED_CODE
	case CP_WORDLIST:
	{
	    char c = *val;
	    switch (c) {
	    case 'c': wl_mode = WL_M_COMBINED; break;
	    case 's': wl_mode = WL_M_SEPARATE; break;
	    default:
		fprintf(stderr, "Unknown wordlist type - '%s'.\n", val);
		exit(EX_ERROR);
	    }
	    if (DEBUG_CONFIG(2))
		fprintf(dbgout, "%s -> '%s'\n", arg->name, val);
	    break;
	}
#endif
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
				const parm_desc *parms, 
				priority_t precedence )
/* returns true if parm is processed, false if not */
{
    const parm_desc *arg;

    if (parms == NULL)
	return false;

    for ( arg = parms; arg->name != NULL; arg += 1 )
    {
	if (DEBUG_CONFIG(2))
	    fprintf(dbgout, "Testing:  %s\n", arg->name);
	if (strcmp(arg->name, line) == 0)
	{
	    bool ok = process_config_parameter(arg, val, precedence);
	    if (ok && DEBUG_CONFIG(1))
		fprintf(dbgout, "%s\n", "   Found it!");
	    return ok;
	}
    }
    return false;
}

bool process_config_option(char *arg, bool warn_on_error, priority_t precedence)
/* returns true if ok, false if error */
{
    const char delim[] = " \t=";
    char *val = NULL;
    bool error = false;

    if (strcspn(arg, delim) < strlen(arg)) { /* if delimiter present */
	val = arg + strcspn(arg, delim);
	*val++ = '\0';
	val += strspn(val, delim);
    }

    if (val == NULL ||
	(! process_config_line(arg, val, usr_parms, precedence ) &&
	 ! process_config_line(arg, val, sys_parms, precedence ) &&
	 ! process_config_line(arg, val, format_parms, precedence )))
    {
	error = true;
 	if (warn_on_error)
	    fprintf(stderr, "Error - bad parameter '%s'\n", arg);
    }

    return error;
}

bool read_config_file(const char *fname, bool tilde_expand, bool warn_on_error, priority_t precedence)
/* returns true if ok, false if error */
{
    bool error = false;
    int lineno = 0;
    FILE *fp;
    char *filename;

    filename = tildeexpand(fname, tilde_expand);

    fp = fopen(filename, "r");

    if (fp == NULL) {
	if (DEBUG_CONFIG(0)) {
	    fprintf(dbgout, "Cannot open %s: %s\n", filename, strerror(errno));
	}
	xfree(filename);
	return false;
    }

    if (DEBUG_CONFIG(0))
	fprintf(dbgout, "Reading %s\n", filename);

    while (!feof(fp))
    {
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

	if (DEBUG_CONFIG(1))
	    fprintf(dbgout, "Testing:  %s\n", buff);

	if (!process_config_option(buff, warn_on_error, precedence))
	    error = true;
    }

    if (ferror(fp)) {
	fprintf(stderr, "Error reading file \"%s\"\n.", filename);
	error = true;
    }

    (void)fclose(fp); /* we're just reading, so fclose should succeed */
    xfree(filename);

    return (error);
}

/* exported */
bool process_config_files(bool warn_on_error)
{
    bool ok = true;
    const char *env = getenv("BOGOTEST");

    if (!suppress_config_file) {
	if (!read_config_file(system_config_file, false, warn_on_error, PR_CFG_SITE))
	    ok = false;
	if (!read_config_file(user_config_file, true, warn_on_error, PR_CFG_USER))
	    ok = false;
    }

    if (env)
	set_bogotest(env);

    return ok;
}
