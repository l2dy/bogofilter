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

#ifdef __riscos__
/* static symbols that trigger UnixLib behaviour */
#include <unixlib/local.h> /* __RISCOSIFY_NO_PROCESS */
int __riscosify_control = __RISCOSIFY_NO_PROCESS;
int __feature_imagefs_is_file = 1;
#endif

bool bf_abspath(const char *path)
{
#ifndef __riscos__
    return (*path == DIRSEP_C);
#else
    return (strchr(path, ':') || strchr(path, '$'));
#endif
}

int bf_mkdir(const char *path, mode_t mode)
{
    int rc;
#ifndef _OS2_
    rc = mkdir(path, mode);
#else
    rc = mkdir((unsigned char *)path);
#endif
    return rc;
}
