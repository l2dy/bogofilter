/* $Id$ */

/*****************************************************************************

NAME:
   fgetsl.c -- fgets(), returning length.

******************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "fgetsl.h"

/* calls exit(2) on read error or when max_size < 2 */
int fgetsl(char *buf, int max_size, FILE *s)
{
    int c = 0;
    char *cp = buf;

    if (max_size < 2) {
	fprintf(stderr, "Invalid buffer size, exiting.\n");
	exit(2);
    }

    if (feof(s))
	return(EOF);

    while ((--max_size > 0) && ((c = getc(s)) != EOF)) {
	*cp++ = c;
	if (c == '\n')
	    break;
    }

    if (c == EOF && ferror(s)) {
	perror("stdin");
	exit(2);
    }

    *cp = '\0'; /* DO NOT ADD ++ HERE! */
    return(cp - buf);
}
