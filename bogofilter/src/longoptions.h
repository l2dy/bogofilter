/* $Id$ */

/*****************************************************************************

NAME:
   longoptions.h -- definitions for longoptions.c

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#ifndef OPTIONS_H
#define OPTIONS_H

/* Definitions */

enum field_e {
    N,		/* no_argument		(or 0) if the option does not take an argument, */
    R,		/* required_argument	(or 1) if the option requires an argument, */
    O		/* optional_argument 	(or 2) if the option takes an optional argument. */
};

typedef enum longopts_e {
    O_BLOCK_ON_SUBNETS = 1000,
    O_CHARSET_DEFAULT,
    O_CONFIG_FILE,
    O_DB_MAX_OBJECTS,
    O_DB_MAX_LOCKS,
    O_DB_PRUNE,
    O_DB_RECOVER,
    O_DB_RECOVER_HARDER,
    O_DB_REMOVE_ENVIRONMENT,
    O_DB_VERIFY,
    O_DB_LOG_AUTOREMOVE,
    O_DB_TXN_DURABLE,
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
    O_WORDLIST
} longopts_t;

/* Global variables */

extern struct option long_options[];

#endif
