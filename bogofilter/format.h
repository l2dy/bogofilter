/* $Id$ */

#ifndef FORMAT_H
#define FORMAT_H

#include "bogoconfig.h"
#include "bogofilter.h"

/* Global variables */

extern const char *spam_header_name;
extern const char *header_format;
extern const char *terse_format;
extern const char *update_log_format;
extern const char *check_log_format;

/* needed by config.c */

extern const parm_desc format_parms[];

/* Prototypes */

extern char *format(const char *frm, double spamicity, rc_t status);

#endif
