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

int verbose = 0;
int logflag = 0;

int thresh_index = 1;
double thresh_stats = 0.2f;		// EVEN_ODDS
double thresh_rtable=0.3f;
char *spam_header_name = "X-Bogosity";

run_t run_type = RUN_NORMAL; 

extern void read_config_file( const char *filename );

int main( int argc, char **argv)
{
    while (--argc > 0)
    {
	char *arg = *++argv;
	if (strcmp(arg, "-v") == 0)
	    verbose = 1;
    }
    read_config_file( "/etc/bogofilter.cf" );
    read_config_file( "./bogofilter.cf" );
    read_config_file( "~/.bogofilter.cf" );
    return 0;
}

bool x_configure_wordlist(const char *val)
{
    return FALSE;
}

int x_init_list(wordlist_t* list, const char* name, const char* filepath, double weight, bool bad, int override, bool ignore)
{
    if (DEBUG_CONFIG(0)) {
	fprintf( stderr, "list:     %p\n", list);
	fprintf( stderr, "name:     %s\n", name);
	fprintf( stderr, "filepath: %s\n", filepath);
	fprintf( stderr, "weight:   %f\n", weight);
	fprintf( stderr, "bad:      %s\n", bad ? "T" : "F" );
	fprintf( stderr, "override: %d\n", override);
	fprintf( stderr, "ignore:   %s\n", ignore ? "T" : "F" );
    }
    return 0;
}

