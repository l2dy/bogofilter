/* $Id$ */

/*****************************************************************************

NAME:
   bogoconfig.h -- prototypes and definitions for bogoconfig.c.

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#ifndef BOGOCONFIG_H
#define BOGOCONFIG_H

#include "config.h"
#include "system.h"

/* Global variables */

extern const char *logtag;
extern const char *spam_header_name;
extern const char *user_config_file;

extern void query_config(void) __attribute__ ((noreturn));
extern void process_args_and_config_file(int argc, char **argv, bool warn_on_error);

#endif
