/* $Id$ */

/*
* NAME:
*    xrealloc.c -- front-end to standard heap manipulation routines, with error checking.
*
* AUTHOR:
*    Gyepi Sam <gyepi@praxis-sw.com>
*
*/

#include <stdlib.h>
#include "xmalloc.h"

void
*xrealloc(void *ptr, size_t size){
   void *x;
   x = realloc(ptr, size);
   if (x == NULL) xmem_error("xrealloc");
   return x;
}

