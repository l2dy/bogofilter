/* (C) 2002 by Matthias Andree, redistributable according to the terms
 * of the GNU General Public License, v2.
 *
 * $Id$
 *
 * $History$
 *
 */

#include <string.h>
#include "xmalloc.h"
#include "xstrdup.h"

char *xstrdup(const char *s) {
    char *t = xmalloc(strlen(s) + 1);
    strcpy(t, s);
    return t;
}
