/* $Id$ */

/*****************************************************************************

NAME:
   sighandler.c -- signal handler

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#include "common.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "sighandler.h"
#include "wordlists.h"

/* Function Definitions */

static void mysignal(int sig, void (*hdl)(int)) {
    struct sigaction sa;

    sa.sa_handler = hdl;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(sig, &sa, NULL)) {
	fprintf(stderr, "Cannot set signal %d handler to %p: %s\n",
		sig, hdl, strerror(errno));
	exit(EX_ERROR);
    }
}

void signal_setup(void)
{
    mysignal(SIGINT,  SIG_IGN);
    mysignal(SIGPIPE, SIG_IGN);
    mysignal(SIGTERM, SIG_IGN);
}
