/* $Id$ */

/*****************************************************************************

NAME:
   system.c -- bogofilter covers for OS/compiler dependent functions.

******************************************************************************/

#include "common.h"
#if defined(__IBMC__) || defined(__IBMCPP__) || defined(__WATCOMC__)
#define _OS2_
#include "direct.h"
#endif

int bf_mkdir(const char *path, mode_t mode)
{
    int rc;
#ifndef _OS2_
    rc = mkdir(path, mode);
#else
    rc = mkdir((unsigned char*)path);
#endif
    return rc;
}
