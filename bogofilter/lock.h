/* $Id$ */
/* $Log$
 * Revision 1.1  2002/09/14 22:15:20  adrian_otto
 * Initial revision
 * */
/*****************************************************************************

NAME:
   lock.h -- Rconstants and declatations for lock.c

AUTHOR:
   Adrian Otto <_NOSPAM_aotto@aotto.com> (remove _NOSPAM_)

DESCRIPTION:
   #define USE_FLOCK (for flock locking)
   #define USE_LOCKF (for POSIX locking)
   If neither of these are defined, fcntl locking is used instead

******************************************************************************/

#include <stdio.h>
#include <signal.h>
#include <config.h>

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif

#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifndef LOCK_TIMEOUT
#define LOCK_TIMEOUT 60
#endif

/****************************************************************************/
/* Constants                                                                */
/****************************************************************************/
//#define LOCK_SH 1       /* Shared lock.  */
//#define LOCK_EX 2       /* Exclusive lock.  */

/****************************************************************************/
/* Function Prototypes                                                      */
/****************************************************************************/
int lock_file(FILE *f, int lock_arg);
int lock_fileno(int f, int lock_arg);
int unlock_file(FILE *f);
int unlock_fileno(int f);
void alarm_signal_handler(int signum);

