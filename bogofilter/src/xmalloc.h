/* $Id$ */

#ifndef XMALLOC_H
#define XMALLOC_H

#ifdef	BF_MALLOC
/* special defines for xmalloc., xcalloc.c, etc */
#ifndef	ENABLE_MEMDEBUG
  #define bf_malloc  malloc
  #define bf_calloc  calloc
  #define bf_realloc realloc
  #define bf_free    free
#else
  #include "memdebug.h"
#endif
#endif

/*@noreturn@*/
void xmem_error(const char *)
#ifdef __GNUC__
 __attribute__((noreturn))
#endif
   ;

/*@noreturn@*/
void xmem_toolong(const char *)
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
