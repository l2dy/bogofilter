/* $Id$ */

/*
* NAME:
*    xmalloc.c -- front-end to standard heap manipulation routines, with error checking.
*
* AUTHOR:
*    Gyepi Sam <gyepi@praxis-sw.com>
*
*/

#include <stdio.h>
#include <stdlib.h>
#include "xmalloc.h"

/*@noreturn@*/
#ifdef __GNUC__
__attribute__ ((noreturn))
#endif
static void mem_error(const char *a)
{
    (void)fprintf(stderr, "%s: Out of memory\n", a);
    abort();
}

void *
xmalloc(size_t size){
	void *x;
	x = malloc(size);
	if (x == NULL) {
	    mem_error("xmalloc"); 
	}
	return x;
}

void
xfree(void *ptr){
  if (ptr) free(ptr);
}

void
*xcalloc(size_t nmemb, size_t size){
   void *x;
   x = calloc(nmemb, size);
   if (x == NULL) {
       mem_error("xcalloc");
   }
   return x;
}

void
*xrealloc(void *ptr, size_t size){
   void *x;
   x = realloc(ptr, size);
   if (x == NULL) mem_error("xrealloc");
   return x;
}

