/* $Id$ */

#ifndef XMALLOC_H
#define XMALLOC_H
#include <stdlib.h>

/*@noreturn@*/
void xmem_error(const char *)
#ifdef __GNUC__
 __attribute__((noreturn))
#endif
   ;

/*@only@*/ /*@out@*/ /*@notnull@*/
void *xmalloc(size_t size);

void xfree(/*@only@*/ /*@null@*/ void *ptr);

/*@only@*/ /*@out@*/ /*@notnull@*/
void *xcalloc(size_t nmemb, size_t size);

/*@only@*/ /*@out@*/ /*@notnull@*/
void *xrealloc(/*@only@*/ void *ptr, size_t size);
#endif
