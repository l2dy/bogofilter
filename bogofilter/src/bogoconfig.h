/* $Id$ */

/*****************************************************************************

NAME:
   bogoconfig.h -- prototypes and definitions for config.c.

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#ifndef BOGOCONFIG_H
#define BOGOCONFIG_H

/* Global variables */

extern const char *logtag;
extern const char *spam_header_name;
extern const char *user_config_file;

extern int  process_args(int argc, char **argv);

void query_config(void);

#endif
