/* parts were taken from autoconf.info */

/*
 * this file shall define the bool and uint32_t types.
 * it shall include inttypes.h and stdbool.h if present
 */

#ifndef SYSTEM_H
#define SYSTEM_H

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

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_SYS_STAT_H
# include <sys/stat.h>
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

/* obtain uint32_t */
#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#elif HAVE_STDINT_H
#include <stdint.h>
#endif

#ifndef HAVE_UINT32_T
#ifdef HAVE_U_INT32_T
typedef u_int32_t uint32_t;
#elif SIZEOF_UNSIGNED_LONG == 4
typedef unsigned long uint32_t;
#elif SIZEOF_UNSIGNED_INT == 4
typedef unsigned int uint32_t;
#elif SIZEOF_UNSIGNED_SHORT == 4
typedef unsigned short uint32_t;
#else
choke me because we do not know how to define uint32_t
#endif
#endif /* HAVE_UINT32_T */

#endif /* SYSTEM_H */
