/* $Id$ */

#include <stdlib.h>
#include "xmalloc.h"

void
*xcalloc(size_t nmemb, size_t size){
   void *x;
   x = calloc(nmemb, size);
   if (x == NULL && (nmemb == 0 || size == 0))
       x = calloc(1, 1);
   if (x == NULL) {
       xmem_error("xcalloc");
   }
   return x;
}
