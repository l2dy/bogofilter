/* $Id$ */

/*****************************************************************************

NAME:
   system.h -- system definitions and prototypes for bogofilter

******************************************************************************/

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

#if HAVE_STRING_H
# if !STDC_HEADERS && HAVE_MEMORY_H
#  include <memory.h>
# endif
# include <string.h>
#endif

#if HAVE_STRINGS_H
#include <strings.h>
#endif

#if !STDC_HEADERS
# if !HAVE_STRCHR
#  define strchr index
# endif
# if !HAVE_STRRCHR
#  define strrchr rindex
# endif

# if !HAVE_MEMCPY
#  define memcpy(d, s, n) bcopy ((s), (d), (n))
# endif
# if !HAVE_MEMMOVE
#  define memmove(d, s, n) bcopy ((s), (d), (n))
# endif
#endif

#if !HAVE_STRLCPY
size_t strlcpy(/*@out@*/ char *dst, const char *src, size_t size);
#endif

#if !HAVE_STRLCAT
size_t strlcat(/*@out@*/ char *dst, const char *src, size_t size);
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
#elif SIZEOF_LONG == 4
typedef unsigned long uint32_t;
#elif SIZEOF_INT == 4
typedef unsigned int uint32_t;
#elif SIZEOF_SHORT == 4
typedef unsigned short uint32_t;
#else
choke me because we do not know how to define uint32_t
#endif
#endif /* HAVE_UINT32_T */
#ifndef HAVE_U_INT32_T
typedef uint32_t u_int32_t;
#endif

#ifndef HAVE_INT32_T
#if SIZEOF_LONG == 4
typedef signed long int32_t;
#elif SIZEOF_INT == 4
typedef signed int int32_t;
#elif SIZEOF_SHORT == 4
typedef signed short int32_t;
#else
choke me because we do not know how to define int32_t
#endif
#endif /* HAVE_INT32_T */

#ifndef HAVE_UINT16_T
#if SIZEOF_SHORT == 2
typedef unsigned short uint16_t;
#else
choke me because we do not know how to define uint16_t
#endif
#endif
#ifndef HAVE_U_INT16_T
typedef uint16_t u_int16_t;
#endif

#ifndef HAVE_INT16_T
#if SIZEOF_SHORT == 2
typedef signed short int16_t;
#else
choke me because we do not know how to define int16_t
#endif
#endif

#ifndef HAVE_U_INT8_T
typedef unsigned char u_int8_t;
#endif

#ifndef HAVE_ULONG
typedef unsigned long ulong;
#endif

#ifndef HAVE_UINT
typedef unsigned int uint;
#endif

/* prevent db.h from redefining the types above */
#undef	__BIT_TYPES_DEFINED__
#define	__BIT_TYPES_DEFINED__ 1

/* splint crutch */
#ifdef __LCLINT__
typedef uint32_t u_int32_t;
typedef uint16_t u_int16_t;
typedef uint8_t u_int8_t;
#define false 0
#define true 1
#endif

#if HAVE_SIZE_T
#if SIZEOF_INT > SIZEOF_SIZE_T
#error "int is wider than size_t. The current code is not designed to work on such systems and needs review."
#endif
#endif

#if HAVE_FCNTL_H
#include <fcntl.h>
#endif

/* dirent.h surroundings */
/* from autoconf.info */
#if HAVE_DIRENT_H
# include <dirent.h>
# define NAMLEN(dirent) strlen((dirent)->d_name)
#else
# define dirent direct
# define NAMLEN(dirent) (dirent)->d_namlen
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef __DGUX__
#undef EX_OK
#endif

/* Ignore __attribute__ if not using GNU CC */
#ifndef __GNUC__
#define __attribute__(a)
#endif

/* system.c - function prototypes */

extern bool bf_abspath(const char *path);
extern int  bf_mkdir(const char *path, mode_t mode);
extern void bf_sleep(long delay);

/* For gcc 2.7.2.1 compatibility */
#ifndef	__attribute__
#define	__attribute__(x)
#endif

#endif /* SYSTEM_H */
