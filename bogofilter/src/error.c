/* $Id$ */

/*****************************************************************************

NAME:
   error.c -- print and log error messages

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>

#include <config.h>
#include "common.h"
#include "globals.h"

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif

#include "error.h"

void print_error( const char *file, unsigned long line, const char *format, ... )
{
    pid_t pid = getpid();
    char message[256];

    va_list ap;
    va_start (ap, format);
    vsnprintf( message, sizeof(message), format, ap );
    va_end (ap);

    fprintf(stderr, "%s: %s\n", progname, message);
#ifdef HAVE_SYSLOG_H
    if (logflag)
	syslog(LOG_INFO, "[%lu] %s:%lu:  %s", (unsigned long)pid,
		file, line, message );
#endif
}
