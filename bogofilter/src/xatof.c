/** \file xatof.c
 * Implements xatof, an easy to use strtod() wrapper.
 *
 * \author Matthias Andree
 * \date 2003
 */

#include "xatox.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>

int xatof(double *d, const char *in) {
    char *end;
    errno = 0;
    *d = strtod(in, &end);
    if (in == end || errno == EINVAL || errno == ERANGE) return 0;
    if (end < in + strlen(in)) return 0;
    return 1;
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
