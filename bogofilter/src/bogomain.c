/* $Id$ */

/*****************************************************************************

NAME:
   bogomain.c -- detect spam and bogons presented on standard input.

AUTHOR:
   Eric S. Raymond <esr@thyrsus.com>
   David Relson    <relson@osagesoftware.com>
   Matthias Andree <matthias.andree@gmx.de>

CONTRIBUTORS:
   Integrate patch by Zeph Hull and Clint Adams to present spamicity in
   X-Spam-Status header in -p (passthrough) mode.

******************************************************************************/

#include "common.h"

#include <stdlib.h>
#include <syslog.h>

#include "getopt.h"

#include "bogoconfig.h"
#include "bogomain.h"
#include "bogofilter.h"
#include "datastore.h"
#include "mime.h"
#include "passthrough.h"
#include "paths.h"
#include "token.h"
#include "wordlists.h"
#include "xmalloc.h"

/* Function Definitions */

ex_t bogomain(int argc, char **argv) /*@globals errno,stderr,stdout@*/
{
    rc_t status;
    ex_t exitcode = EX_OK;

    dbgout = stderr;

    progtype = build_progtype(progname, DB_TYPE);

    process_parameters(argc, argv, true);
    argc -= optind;
    argv += optind;

#ifdef	HAVE_SYSLOG_H
    if (logflag)
	openlog("bogofilter", LOG_PID, LOG_MAIL);
#endif

    output_setup();

    /* open all wordlists */
    open_wordlists((run_type == RUN_NORMAL) ? DS_READ : DS_WRITE);

    status = bogofilter(argc, argv);

    switch (status) {
    case RC_SPAM:	exitcode = EX_SPAM;	break;
    case RC_HAM:	exitcode = EX_HAM;	break;
    case RC_UNSURE:	exitcode = EX_UNSURE;	break;
    case RC_OK:		exitcode = EX_OK;	break;
    default:
	fprintf(dbgout, "Unexpected status code - %d\n", (int)status);
	return (EX_ERROR);
    }

    if (nonspam_exits_zero && exitcode != EX_ERROR)
	exitcode = EX_OK;

    close_wordlists();
    free_wordlists();

    output_cleanup();

    /* cleanup storage */
    mime_cleanup();
    token_cleanup();

    MEMDISPLAY;

#ifdef	HAVE_SYSLOG_H
    if (logflag)
	closelog();
#endif

    free(progtype);

    return (exitcode);
}

/* End */
