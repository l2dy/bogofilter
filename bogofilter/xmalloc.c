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


void *
xmalloc(size_t size){
	void *x;
	x = malloc(size);
	if (x == NULL) {
	    xmem_error("xmalloc"); 
	}
	return x;
}

void
xfree(void *ptr){
  if (ptr) free(ptr);
}

