/* $Id$ */

#include <stdio.h>
#include "xmalloc.h"

/*@noreturn@*/
void xmem_error(const char *a)
{
    (void)fprintf(stderr, "%s: Out of memory\n", a);
    abort();
}
