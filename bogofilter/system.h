/* $Id$ */

/* from autoconf.info */

#ifndef SYSTEM_H
#define SYSTEM_H

#include "config.h"

#if HAVE_STDBOOL_H
# include <stdbool.h>
/*
# undef	true
# undef	false
# define false (bool) 0
# define true  (bool) 1
*/
#else
# if ! HAVE__BOOL
#  ifdef __cplusplus
typedef bool _Bool;
#  else
typedef unsigned char _Bool;
#  endif
# endif
# define bool _Bool
# define false (bool) 0
# define true  (bool) 1
# define __bool_true_false_are_defined 1
#endif

#if STDC_HEADERS
# include <string.h>
#else
# if !HAVE_STRCHR
#  define strchr index
# endif
# if !HAVE_STRRCHR
#  define strrchr rindex
# endif
char *strchr (), *strrchr ();
# if !HAVE_MEMCPY
#  define memcpy(d, s, n) bcopy ((s), (d), (n))
#  define memmove(d, s, n) bcopy ((s), (d), (n))
# endif
#endif


#endif
