/* $Id$ */

/*****************************************************************************

NAME:
   bftypes.h -- type definitions for bogofilter

******************************************************************************/

/* parts were taken from autoconf.info */

/*
 * this file shall define the bool and uint32_t types.
 * it shall include inttypes.h and stdbool.h if present
 */

#ifndef BFTYPES_H
#define BFTYPES_H

#ifndef	CONFIG_H
# define  CONFIG_H
# include "config.h"
#endif

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

/* SunOS 4.1.X needs this */
#ifndef HAVE_SSIZE_T
typedef int ssize_t;
#endif

#ifndef HAVE_RLIM_T
typedef int rlim_t;
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

typedef uint32_t YYYYMMDD;	/* date as YYYYMMDD */

#ifdef HAVE_SIZE_T
#if SIZEOF_INT > SIZEOF_SIZE_T
#error "int is wider than size_t. The current code is not designed to work on such systems and needs review."
#endif
#endif

#endif /* BFTYPES_H */
