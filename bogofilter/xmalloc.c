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

#define mem_error(a) do { fprintf(stderr, a ": Out of memory\n"); abort(); } while(0)

void *
xmalloc(size_t size){
	char *x;
	if (!(x = malloc(size))) mem_error("xmalloc"); 
	return (void *)x;
}

void
xfree(void *ptr){
  if (ptr) free(ptr);
}

void
*xcalloc(size_t nmemb, size_t size){
   char *x;
   if (!(x = calloc(nmemb, size))) mem_error("xcalloc");
   return (void *)x;
}

void
*xrealloc(void *ptr, size_t size){
   char *x;
   if (!(x = realloc(ptr, size))) mem_error("xrealloc");
   return(void *)x;
}

