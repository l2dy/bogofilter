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
const char *const system_config_file = "<Bogofilter$Dir>.bogofilter/cf";
#endif

bool bf_abspath(const char *path)
{
#if	!defined(__OS2__) && !defined(__riscos__)
    return (bool) (*path == DIRSEP_C);
#else
  #ifdef	__OS2__
    return (bool) strchr(path, ':');
  #endif
  #ifdef	__riscos__
    return (bool) (strchr(path, ':') || strchr(path, '$') || strchr(path, '#') ||
		   strchr(path, '@') || strchr(path, '%') || strchr(path, '&'));
  #endif
#endif
}

void bf_sleep(long delay)
{
#ifndef _OS2_
    struct timeval timeval;
    timeval.tv_sec  = delay / 1000000;
    timeval.tv_usec = delay % 1000000;
    select(0,NULL,NULL,NULL,&timeval);
#else
/*APIRET DosSleep(ULONG  msec )  */
    DosSleep(1);
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
