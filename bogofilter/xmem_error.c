/* $Id$ */

#include <stdio.h>
#include "xmalloc.h"

/*@noreturn@*/
#ifdef __GNUC__
__attribute__ ((noreturn))
#endif
void xmem_error(const char *a)
{
    (void)fprintf(stderr, "%s: Out of memory\n", a);
    abort();
}
