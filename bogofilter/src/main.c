/* $Id$ */

/*****************************************************************************

NAME:
   main.c -- a wrapper for bogomain

AUTHOR:
   Eric S. Raymond <esr@thyrsus.com>

******************************************************************************/

#include "common.h"

#include <stdlib.h>

#include "bogomain.h"

const char *progname = "bogofilter";

/* Function Definitions */

int main(int argc, char **argv) /*@globals errno,stderr,stdout@*/
{
    ex_t exitcode = bogomain(argc, argv);

    exit(exitcode);
}

/* End */
