/* $Id$ */

/*****************************************************************************

NAME:
   error.h -- definitions and prototypes for error.c

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#ifndef ERROR_H
#define ERROR_H

#define PRINT_ERROR(fmt...)	print_error(__FILE__, __LINE__, fmt)
extern void print_error( const char *file, int line, const char *format, ... )
#ifdef __GNUC__
    __attribute__ ((format(printf,3,4)))
#endif
    ;

#endif
