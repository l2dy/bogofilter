/** \file xatoi.c
 * Implements xatoi, an easy to use atoi() replacement with error
 * checking.
 *
 * \author Matthias Andree
 * \date 2003
 */

#include "xatox.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

int xatoi(int *i, const char *in) {
    char *end;
    long d;
    errno = 0;
    d = strtol(in, &end, 10);
    if (in == end || errno == EINVAL || errno == ERANGE) return 0;
    if (d > INT_MAX || d < INT_MIN) { errno = ERANGE; return 0; }
    if (end < in + strlen(in)) return 0;
    *i = d;
    return 1;
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
