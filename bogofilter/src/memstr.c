#include "memstr.h"

/* (C) 2004 Matthias Andree. License: GNU GPL v2 */

void *memstr(const void *hay, size_t n, const unsigned char *needle)
{
    unsigned const char *haystack = hay;
    size_t l = strlen((const char *)needle);

    while (n >= l) {
	if (0 == memcmp(haystack, needle, l))
	    return (void *)haystack;
	haystack++;
	n--;
    }
    return (void *)0;
}
