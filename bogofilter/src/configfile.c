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

#include "getopt.h"

#include "bogoconfig.h"
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
char 	*config_file_name;

/*---------------------------------------------------------------------------*/

/*  remove trailing comment from the line.
 */
void remove_comment(const char *line)
{
    char *tmp = strchr(line, '#');
    if (tmp != NULL) {
	tmp -= 1;
	while (tmp > line && isspace((unsigned char)*tmp))
	    tmp -= 1;
	*(tmp+1) = '\0';
    }
    return;
}

bool process_config_option(const char *arg, bool warn_on_error, priority_t precedence)
{
    uint pos;
    bool ok = true;

    char *val = NULL;
    char *opt = xstrdup(arg);
    const char delim[] = " \t=";

    pos = strcspn(arg, delim);
    if (pos < strlen(arg)) { 		/* if delimiter present */
	val = opt + pos;
	*val++ = '\0';
	val += strspn(val, delim);
    }

    if (val == NULL || 
	!process_config_option_as_arg(opt, val, precedence)) {
	ok = false;
	if (warn_on_error)
	    fprintf(stderr, "Error - bad parameter '%s'\n", arg);
    }

    xfree(opt);
    return ok;
}

bool process_config_option_as_arg(const char *opt, const char *val, priority_t precedence)
{
    int idx = 0;
    size_t len = strlen(opt);

    for (idx = 0; ; idx += 1) {
	const char *s = long_options[idx].name;
	if (s == NULL)
	    break;
	if (strlen(s) != len || strncasecmp(s, opt, len) != 0)
	    continue;
	if (strcmp(val, "''") == 0)
	    val = "";
	process_arg(long_options[idx].val, s, val, precedence, PASS_2_CFG);
	return true;
    }

    return false;
}

bool read_config_file(const char *fname, bool tilde_expand, bool warn_on_error, priority_t precedence)
{
    bool ok = true;
    int lineno = 0;
    FILE *fp;

    if (config_file_name != NULL)
	xfree(config_file_name);
    config_file_name = tildeexpand(fname, tilde_expand);

    fp = fopen(config_file_name, "r");

    if (fp == NULL) {
	xfree(config_file_name);
	config_file_name = NULL;
	return false;
    }

    if (DEBUG_CONFIG(0))
	fprintf(dbgout, "Reading %s\n", config_file_name);

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
	    ok = false;
    }

    if (ferror(fp)) {
	fprintf(stderr, "Error reading file \"%s\"\n.", config_file_name);
	ok = false;
    }

    (void)fclose(fp); /* we're just reading, so fclose should succeed */

    return ok;
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
