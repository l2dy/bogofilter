/* $Id$ */

/** \file xatof.c
 * Implements xatof, an easy to use strtod() replacement with error
 * checking.
 *
 * \author Matthias Andree
 * \date 2003
 */

#include "xatox.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

int xatof(double *d, const char *in) {
    char *end;
    double val;

    errno = 0;
    val = strtod(in, &end);
    if (in == end || errno == EINVAL || errno == ERANGE) return 0;

    if (*end == 'f')		/* allow terminal 'f' */
	end += 1;

    while (isspace(*end))
	end += 1;

    if (*end == '#' || *end == '\0' || end == in + strlen(in)) 
    {
	*d = val;
	return 1;
    }
    else
	return 0;
}

#if MAIN
#include <stdio.h>

int main(int argc, char **argv) {
    int i;
    for (i = 1; i < argc; i++) {
	double d;
	int s = xatof(&d, argv[i]);
	printf("%s -> errno=%d status=%d double=%g\n", argv[i], errno, s, d);
    }
    exit(0);
}
#endif
