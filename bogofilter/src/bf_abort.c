#include "common.h"
#include "system.h"
#include <stdlib.h>
#include "wordlists.h"

void bf_abort(void)
{
    close_wordlists(false);
    abort();
}
