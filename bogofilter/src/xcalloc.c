/* $Id$ */

/*
* NAME:
*    xcalloc.c -- front-end to standard heap manipulation routines, with error checking.
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
*xcalloc(size_t nmemb, size_t size){
   void *ptr;
   ptr = calloc(nmemb, size);
   if (ptr == NULL && (nmemb == 0 || size == 0))
       ptr = calloc(1, 1);
   if (ptr == NULL)
       xmem_error("xcalloc");
   return ptr;
}
