#include <stdlib.h>
#include "xmalloc.h"

void
*xrealloc(void *ptr, size_t size){
   void *x;
   x = realloc(ptr, size);
   if (x == NULL) xmem_error("xrealloc");
   return x;
}

