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

#include <config.h>
#include "common.h"

#include "method.h"
#include "wordlists.h"
#include "xmalloc.h"

#include "bogoconfig.h"

const char *progname = "configtest";

double robs, robx;
double ham_cutoff;

#ifndef	DEBUG_CONFIG
#define DEBUG_CONFIG(level)	(verbose > level)
#endif

/* Dummy struct definitions to support config.c */
method_t graham_method = {
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
} ;
method_t rf_robinson_method = {
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
} ;
method_t rf_fisher_method = {
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
} ;

#ifdef COMPILE_DEAD_CODE
static bool x_configure_wordlist(const char *val)
{
    return false;
}

static int x_init_list(wordlist_t* list, const char* name, const char* filepath, double weight, bool bad, int override, bool ignore)
{
    if (DEBUG_CONFIG(0)) {
	fprintf( stderr, "list:     %p\n", (void *)list);
	fprintf( stderr, "name:     %s\n", name);
	fprintf( stderr, "filepath: %s\n", filepath);
	fprintf( stderr, "weight:   %f\n", weight);
	fprintf( stderr, "bad:      %s\n", bad ? "T" : "F" );
	fprintf( stderr, "override: %d\n", override);
	fprintf( stderr, "ignore:   %s\n", ignore ? "T" : "F" );
    }
    return 0;
}
#endif

int main( int argc, char **argv)
{
    verbose = 0;
    logflag = 0;

    while (--argc > 0)
    {
	char *arg = *++argv;
	if (strcmp(arg, "-v") == 0)
	    verbose = 1;
    }
    if ( !process_config_files(false) )
	exit(2);
    /* read_config_file("./bogofilter.cf", true, false); */
    return 0;
}
