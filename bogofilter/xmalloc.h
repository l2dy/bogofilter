#ifndef XMALLOC_H_GUARD
#define XMALLOC_H_GUARD
#include <stdlib.h>

void *xmalloc(size_t size);
void xfree(void *ptr);
void *xcalloc(size_t nmemb, size_t size);
void *xrealloc(void *ptr, size_t size);

#endif

