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

#include "config.h"

#include "xmalloc.h"

#ifdef	ENABLE_MEMDEBUG
#include "memdebug.h"
#endif

void
*xrealloc(void *ptr, size_t size){
   ptr = realloc(ptr, size);
   if (ptr == NULL && size == 0)
       ptr = calloc(1, 1);
   if (ptr == NULL)
       xmem_error("xrealloc");
   return ptr;
}
