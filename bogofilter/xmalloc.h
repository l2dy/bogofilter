/* $Id$ */

#ifndef XMALLOC_H_GUARD
#define XMALLOC_H_GUARD
#include <stdlib.h>

/*@only@*//*@out@*//*@notnull@*/
void *xmalloc(size_t size);

void xfree(/*@only@*//*@null@*/ void *ptr);

/*@only@*//*@out@*//*@notnull@*/
void *xcalloc(size_t nmemb, size_t size);

/*@only@*//*@out@*//*@notnull@*/
void *xrealloc(/*@only@*/ void *ptr, size_t size);

#endif

