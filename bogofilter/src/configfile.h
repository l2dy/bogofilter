/* $Id$ */

/*****************************************************************************

NAME:
   configfile.h -- prototypes and definitions for bogoconfig.c.

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#ifndef CONFIGFILE_H
#define CONFIGFILE_H

/* Definitions */

typedef enum arg_pass_e {
    PASS_1_CLI = 1,		/* 1 - first command line pass  */
    PASS_2_CFG = 2,		/* 2 - config file options ...  */
    PASS_3_CLI = 3		/* 3 - second command line pass */
} arg_pass_t;

enum field_e {
    N,		/* no_argument		(or 0) if the option does not take an argument, */
    R,		/* required_argument	(or 1) if the option requires an argument, */
    O		/* optional_argument 	(or 2) if the option takes an optional argument. */
};

typedef enum longopts_e {
    O_IGNORE = 1000,
    O_BLOCK_ON_SUBNETS,
    O_CHARSET_DEFAULT,
    O_DB_MAX_OBJECTS,
    O_DB_MAX_LOCKS,
#ifdef	FUTURE_DB_OPTIONS
    O_DB_LOG_AUTOREMOVE,
    O_DB_TXN_DURABLE,
#endif
    O_NS_ESF,
    O_SP_ESF,
    O_HAM_CUTOFF,
    O_HEADER_FORMAT,
    O_LOG_HEADER_FORMAT,
    O_LOG_UPDATE_FORMAT,
    O_MIN_DEV,
    O_REPLACE_NONASCII_CHARACTERS,
    O_ROBS,
    O_ROBX,
    O_SPAM_CUTOFF,
    O_SPAM_HEADER_NAME,
    O_SPAM_SUBJECT_TAG,
    O_SPAMICITY_FORMATS,
    O_SPAMICITY_TAGS,
    O_STATS_IN_HEADER,
    O_TERSE,
    O_TERSE_FORMAT,
    O_THRESH_UPDATE,
    O_TIMESTAMP,
    O_UNSURE_SUBJECT_TAG,
    O_USER_CONFIG_FILE,
    O_WORDLIST,
    O_REMOVE_ENVIRONMENT
} longopts_t;

/* Global variables */

extern char *config_file_name;

extern void remove_comment(const char *line);
extern bool process_config_files(bool warn_on_error);
extern bool process_config_option(const char *arg, bool warn_on_error, priority_t precedence);
extern bool read_config_file(const char *fname, bool tilde_expand, bool warn_on_error, priority_t precedence);
extern bool process_config_option_and_val(const char *name, const char *val, bool warn_on_error, priority_t precedence);
extern bool process_config_option_as_arg(const char *arg, const char *val, priority_t precedence);

#endif
