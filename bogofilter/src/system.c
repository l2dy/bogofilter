/* $Id$ */

/*****************************************************************************

NAME:
   system.c -- bogofilter covers for OS dependent functions.

******************************************************************************/

#include "common.h"

int bf_mkdir(const char *path, mode_t mode)
{
    int rc;
#if !defined(__IBMC__) && !defined(__IBMCPP__)
    rc = mkdir(path, mode);
#else
    rc = mkdir(path);
#endif
    return rc;
}
