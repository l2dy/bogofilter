/* from autoconf.info */

#ifndef SYSTEM_H
#define SYSTEM_H

#include <string.h>

#include "config.h"

#if HAVE_STDBOOL_H
# include <stdbool.h>
#else
# if ! HAVE__BOOL
#  ifdef __cplusplus
typedef bool _Bool;
#  else
typedef unsigned char _Bool;
#  endif
# endif
# define bool _Bool
# define false 0
# define true 1
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

# if !HAVE_MEMCPY
#  define memcpy(d, s, n) bcopy ((s), (d), (n))
#  define memmove(d, s, n) bcopy ((s), (d), (n))
# endif
#endif

#if !HAVE_STRLCPY
size_t strlcpy(char *dst, const char *src, size_t size);
#endif

#if !HAVE_STRLCAT
size_t strlcat(char *dst, const char *src, size_t size);
#endif

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#endif
