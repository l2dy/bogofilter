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
   void *x;
   if (size != 0)
       x = realloc(ptr, size);
   else {
       xfree(ptr);
       x = xcalloc(1, size);
   }
   if (x == NULL) xmem_error("xrealloc");
   return x;
}
