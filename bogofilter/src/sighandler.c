/* $Id$ */

/*****************************************************************************

NAME:
   sighandler.c -- signal handler

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#include "common.h"

#include <signal.h>
#include <stdlib.h>

#include "sighandler.h"
#include "wordlists.h"

/* Function Definitions */

static void signal_handler(int sig)
{
    close_wordlists(word_lists, true);
    exit(sig);
}

void signal_setup(void)
{
    sigset_t mask;

    sigemptyset(&mask);

#ifdef	SIGINT
    signal(SIGINT,  signal_handler);	/*  2 */
    sigaddset(&mask, SIGINT);
#endif

#ifdef	SIGKILL
    signal(SIGKILL, signal_handler);	/*  9 */
    sigaddset(&mask, SIGKILL);
#endif

#ifdef	SIGTERM
    signal(SIGTERM, signal_handler);	/* 15 */
    sigaddset(&mask, SIGTERM);
#endif

    sigprocmask(SIG_UNBLOCK, &mask, 0);
}
