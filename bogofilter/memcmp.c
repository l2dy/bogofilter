/* replacement memcmp.c */

#include <string.h>

int memcmp(const void *s1, const void *s2, size_t n) {
    register unsigned const char *p1=s1, *p2=s2;
    int d;

    while(n-- > 0) {
	d = *p1++ - *p2++;
	if (d != 0) return d;
    }
    return 0;
}
