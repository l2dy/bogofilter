/* $Id$ */

/*****************************************************************************

NAME:
   longoptions.c -- define long options

   2004-12-28 - split out from bogoconfig.c so it can be used by
		bogolexer and bogoutil.

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#include "common.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "getopt.h"
#include "longoptions.h"

/*---------------------------------------------------------------------------*/

struct option long_options[] = {
    { "classify-files",			N, 0, 'B' },
    { "syslog-tag",			N, 0, 'L' },
    { "classify-mbox",			N, 0, 'M' },
    { "unregister-nonspam",		N, 0, 'N' },
    { "dataframe",			N, 0, 'R' },
    { "unregister-spam",		N, 0, 'S' },
    { "fixed-terse-format",		N, 0, 'T' },
    { "report-unsure",			N, 0, 'U' },
    { "version",			N, 0, 'V' },
    { "classify-stdin",			N, 0, 'b' },
    { "bogofilter-dir",			R, 0, 'd' },
    { "nonspam-exits-zero",		N, 0, 'e' },
    { "help",				N, 0, 'h' },
    { "use-syslog",			N, 0, 'l' },
    { "register-ham",			N, 0, 'n' },
    { "passthrough",			N, 0, 'p' },
    { "register-spam",			N, 0, 's' },
    { "update-as-scored",		N, 0, 'u' },
    { "timestamp-date",			N, 0, 'y' },
    { "config-file",			R, 0, O_CONFIG_FILE },
    { "no-config-file",			N, 0, 'C' },
    { "debug-flags",			R, 0, 'x' },
    { "debug-to-stdout",		N, 0, 'D' },
    { "no-header-tags",			N, 0, 'H' },
    { "input-file",			N, 0, 'I' },
    { "output-file",			N, 0, 'O' },
    { "query",				N, 0, 'Q' },
    { "verbosity",			N, 0, 'v' },
    { "block-on-subnets",		R, 0, O_BLOCK_ON_SUBNETS },
    { "charset-default",		R, 0, O_CHARSET_DEFAULT },
    { "db-cachesize",			N, 0, 'k' },
    { "db-prune",                       R, 0, O_DB_PRUNE },
    { "db-recover",                     R, 0, O_DB_RECOVER },
    { "db-recover-harder",              R, 0, O_DB_RECOVER_HARDER },
    { "db-remove-environment",		R, 0, O_DB_REMOVE_ENVIRONMENT },
    { "db-verify",                      R, 0, O_DB_VERIFY },
#ifdef	HAVE_DECL_DB_CREATE
    { "db-lk-max-locks",		R, 0, O_DB_MAX_LOCKS },
    { "db-lk-max-objects",		R, 0, O_DB_MAX_OBJECTS },
#ifdef	FUTURE_DB_OPTIONS
    { "db-log-autoremove",		R, 0, O_DB_LOG_AUTOREMOVE },
    { "db-txn-durable",			R, 0, O_DB_TXN_DURABLE },
#endif
#endif
    { "ns-esf",				R, 0, O_NS_ESF },
    { "sp-esf",				R, 0, O_SP_ESF },
    { "ham-cutoff",			R, 0, O_HAM_CUTOFF },
    { "header-format",			R, 0, O_HEADER_FORMAT },
    { "log-header-format",		R, 0, O_LOG_HEADER_FORMAT },
    { "log-update-format",		R, 0, O_LOG_UPDATE_FORMAT },
    { "min-dev",			R, 0, O_MIN_DEV },
    { "replace-nonascii-characters",	R, 0, O_REPLACE_NONASCII_CHARACTERS },
    { "robs",				R, 0, O_ROBS },
    { "robx",				R, 0, O_ROBX },
    { "spam-cutoff",			R, 0, O_SPAM_CUTOFF },
    { "spam-header-name",		R, 0, O_SPAM_HEADER_NAME },
    { "spam-subject-tag",		R, 0, O_SPAM_SUBJECT_TAG },
    { "spamicity-formats",		R, 0, O_SPAMICITY_FORMATS },
    { "spamicity-tags",			R, 0, O_SPAMICITY_TAGS },
    { "stats-in-header",		R, 0, O_STATS_IN_HEADER },
    { "terse",				R, 0, O_TERSE },
    { "terse-format",			R, 0, O_TERSE_FORMAT },
    { "thresh-update",			R, 0, O_THRESH_UPDATE },
    { "timestamp",			R, 0, O_TIMESTAMP },
    { "unsure-subject-tag",		R, 0, O_UNSURE_SUBJECT_TAG },
    { "user-config-file",		R, 0, O_USER_CONFIG_FILE },
    { "wordlist",			R, 0, O_WORDLIST },
    { NULL,				0, 0, 0 }
};
