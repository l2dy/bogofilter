#ifndef MEMSTR_H
#define MEMSTR_H

/* (C) 2004 Matthias Andree. License: GNU GPL v2 */

#include <string.h>

/** find needle in haystack (which is treated as unsigned char *). */
void *memstr(const void *haystack, size_t n, const unsigned char *needle);

#endif
