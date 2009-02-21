#ifndef	_LISTSORT_H
#define	_LISTSORT_H

#define	LISTSORT

#include "bftypes.h"

typedef int fcn_compare(const void *a, const void *b);

extern void *listsort(void *list, fcn_compare *compare);

#endif
