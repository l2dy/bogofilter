/* $Id$ */

/*
* NAME:
*    xmalloc.c -- front-end to standard heap manipulation routines, with error checking.
*
* AUTHOR:
*    Gyepi Sam <gyepi@praxis-sw.com>
*
*/

#include <stdlib.h>

#include "config.h"

#include "xmalloc.h"
#ifdef	ENABLE_MEMDEBUG
#include "memdebug.h"
#endif

void *
xmalloc(size_t size){
	void *x;
	x = malloc(size);
	if (x == NULL && size == 0)
	    x = malloc(1);
	if (x == NULL) {
	    xmem_error("xmalloc"); 
	}
	return x;
}

void
xfree(void *ptr){
  if (ptr) free(ptr);
}

