/* $Id$ */

/** \file xatoi.c
 * Implements xatoi, an easy to use atoi() replacement with error
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
#include <limits.h>

int xatoi(int *i, const char *in) {
    char *end;
    long val;

    errno = 0;
    val = strtol(in, &end, 10);
    if (in == end || errno == EINVAL || errno == ERANGE) return 0;
    if (val > INT_MAX || val < INT_MIN) { errno = ERANGE; return 0; }

    while (isspace(*end))
	end += 1;

    if (*end == '#' || *end == '\0' || end == in + strlen(in)) 
    {
	*i = val;
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
	int d;
	int s = xatoi(&d, argv[i]);
	printf("%s -> errno=%d status=%d int=%d\n", argv[i], errno, s, d);
    }
    exit(0);
}
#endif
