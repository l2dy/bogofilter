#include "datastore_qdbm.h"

int cmpkey(const char *aptr, int asiz, const char *bptr, int bsiz)
{
    int aiter, biter;

    for (aiter = 0, biter = 0; aiter < asiz && biter < bsiz; ++aiter, ++biter) {
	if (aptr[aiter] != bptr[biter])
	    return (aptr[aiter] < bptr[biter]) ? -1 : 1;
    }

    if (aiter == asiz && biter == bsiz)
	return 0;

    return (aiter == asiz) ? -1 : 1;
}


