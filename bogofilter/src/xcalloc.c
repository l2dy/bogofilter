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
   void *prt;
   prt = calloc(nmemb, size);
   if (prt == NULL && (nmemb == 0 || size == 0))
       prt = calloc(1, 1);
   if (prt == NULL)
       xmem_error("xcalloc");
   return ptr;
}
