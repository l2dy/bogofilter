/* $Id$ */
/*
 * $Log$
 * Revision 1.4  2002/09/23 11:38:11  m-a
 * Fix missing right paranthesis.
 *
 * Revision 1.3  2002/09/23 11:31:53  m-a
 * Unnest comments, and move $ line down by one to prevent CVS from adding nested comments again.
 *
 * Revision 1.2  2002/09/17 06:23:46  adrian_otto
 * Changed HAVE_FCNTL_H to HAVE_FCNTL and added safeguard chacks to make sure
 * the lock contants LOCK_EX and LOCK_SH get defined.
 *
 * Revision 1.1.1.1  2002/09/14 22:15:20  adrian_otto
 * 0.7.3 Base Source
 * */
/****************************************************************************/
/*                                                                          */
/* NAME:                                                                    */
/*   lock.c -- Locking and unlocking routines for flock, lockf, and fcntl   */
/*                                                                          */
/* AUTHOR:                                                                  */
/*   Adrian Otto <_NOSPAM_aotto@aotto.com> (remove _NOSPAM_)                */
/*                                                                          */
/* DESCRIPTION:                                                             */
/*   #define HAVE_FLOCK (for flock locking)                                 */
/*   #define HAVE_LOCKF (for POSIX locking)                                 */
/*   If neither of these are defined, fcntl locking is used instead         */
/*                                                                          */
/****************************************************************************/
#include "lock.h"
#include <config.h>
#include <string.h>

/****************************************************************************/
/* main()                                                                   */
/*                                                                          */
/****************************************************************************/
/*
int main(void) {
	return(0); 
}
*/

/****************************************************************************/
/* lock_file()                                                              */
/*                                                                          */
/* Set a lock (or locks) on a file                                          */
/****************************************************************************/
int lock_file (FILE *f, int lock_arg) {
	return(lock_fileno(fileno(f), lock_arg));
}

/****************************************************************************/
/* lock_fileno()                                                            */
/*                                                                          */
/* Set a lock (or locks) on a fileno                                        */
/****************************************************************************/
int lock_fileno (int f, int lock_arg) {

#ifndef HAVE_LOCKF
#ifdef HAVE_FCNTL
	/* persist a struct initialized the first time this function runs */
	static struct flock lock_info = { F_RDLCK, SEEK_SET, 0, 0 };

	if(lock_arg == LOCK_EX) {           /* Excludive lock requested */
		lock_info.l_type = F_RWLCK; /* Use write lock instead */
	}
#endif /* HAVE_FCNTL */
#endif /* HAVE_LOCKF */

#ifdef HAVE_SIGNAL_H
#ifdef HAVE_UNISTD_H
	/* Set a timeout in case we can't get the lock */
	signal(SIGALRM, *alarm_signal_handler);
	alarm(LOCK_TIMEOUT);
#endif /* HAVE_UNISTD_H */
#endif /* HAVE_SIGNAL_H */


#ifdef HAVE_FLOCK

	/* flock locking */
	#ifdef HAVE_SYSLOG_H
	syslog(LOG_DEBUG, "trying flock lock");
	#endif /* HAVE_SYSLOG_H */
	if(flock(f, LOCK_NB|lock_arg) != 0) { /* try nonblocking lock */
		signal(SIGALRM, *alarm_signal_handler);
		alarm(LOCK_TIMEOUT); /* set alarm timer, notice on timeout */
		#ifdef HAVE_SYSLOG_H
		syslog(LOG_DEBUG, "file already flock'ed, now waiting.");
		#endif /* HAVE_SYSLOG_H */
		flock(f, lock_arg); /* block until locked */
		alarm(0); /* turn off the alarm timer */
		signal(SIGALRM, SIG_DFL); /* return signal to default */
		#ifdef HAVE_SYSLOG_H
		syslog(LOG_DEBUG, "got flock on the file, continuing.");
		#endif /* HAVE_SYSLOG_H */
	} 

#elif HAVE_LOCKF

	/* POSIX locking */
	#ifdef HAVE_SYSLOG_H
	syslog(LOG_DEBUG, "trying POSIX lock");
	#endif /* HAVE_SYSLOG_H */
	if(lockf(f, F_TLOCK, 0) != 0) { /* try nonblocking lock */
		signal(SIGALRM, *alarm_signal_handler);
		alarm(LOCK_TIMEOUT);            /* set timeout */
		#ifdef HAVE_SYSLOG_H
		syslog(LOG_DEBUG, "file already POSIX locked, now waiting.");
		#endif /* HAVE_SYSLOG_H */
		lockf(f, F_LOCK, 0);    /* block until locked */
		alarm(0);                       /* turn off alarm timer */
		signal(SIGALRM, SIG_DFL);       /* return signal to default */
		#ifdef HAVE_SYSLOG_H
		syslog(LOG_DEBUG, "got POSIX lock on the file, continuing.");
		#endif /* HAVE_SYSLOG_H */
	} 

#elif HAVE_FCNTL

	/* fcntl locking */
	#ifdef HAVE_SYSLOG_H
	syslog(LOG_DEBUG, "trying fcntl lock");
	#endif /* HAVE_SYSLOG_H */
	if(fcntl(f, F_SETLKW, &lock_info) != 0) { /* nonblocking */
		signal(SIGALRM, *alarm_signal_handler);
		alarm(LOCK_TIMEOUT);                      /* set alarm timer */
		#ifdef HAVE_SYSLOG_H
		syslog(LOG_DEBUG, "file already fcntl locked, now waiting.");
		#endif /* HAVE_SYSLOG_H */
		fcntl(f, F_SETLKW, &lock_info);   /* get lock */
		alarm(0);                                 /* turn off alarm */
		signal(SIGALRM, SIG_DFL);                 /* signal default */
		#ifdef HAVE_SYSLOG_H
		syslog(LOG_DEBUG, "got fcntl lock on the file, continuing."); 
		#endif /* HAVE_SYSLOG_H */
	}

#endif /* HAVE_FLOCK */

#ifdef HAVE_SIGNAL_H
#ifdef HAVE_UNISTD_H
	signal(SIGALRM, SIG_DFL);	/* Return the original signal handler */
	alarm(0); 			/* Clear the alarm */
#endif /* HAVE_UNISTD_H */
#endif /* HAVE_SIGNAL_H */

	return(0);
}

/****************************************************************************/
/* unlock_file()                                                            */
/*                                                                          */
/* Release a lock (or locks) on a file                                      */
/****************************************************************************/
int unlock_file (FILE *f) {
	return(unlock_fileno(fileno(f)));
}

/****************************************************************************/
/* unlock_fileno()                                                          */
/*                                                                          */
/* Release a lock (or locks) on a fileno                                    */
/****************************************************************************/
int unlock_fileno (int f) {

#ifndef HAVE_LOCKF
#ifdef HAVE_FCNTL
	/* persist a struct initialized the first time this function runs */
	static struct flock lock_info = { F_UNLCK, SEEK_SET, 0, 0 };
#endif /* HAVE_FCNTL */
#endif /* HAVE_LOCKF */

#ifdef HAVE_FLOCK

	/* flock locking */
	flock(f, LOCK_UN); /* release the flock() lock */
	#ifdef HAVE_SYSLOG_H
	syslog(LOG_DEBUG, "released flock lock");
	#endif /* HAVE_SYSLOG_H */

#elif HAVE_LOCKF

	/* POSIX locking */
	lockf(f, F_ULOCK, 0);
	#ifdef HAVE_SYSLOG_H
	syslog(LOG_DEBUG, "released POSIX lock");
	#endif /* HAVE_SYSLOG_H */

#elif HAVE_FCNTL

	/* fcntl locking */
	fcntl(f, F_SETLKW, &lock_info); /* release fcntl lock */
	#ifdef HAVE_SYSLOG_H
	syslog(LOG_DEBUG, "released fcntl lock");
	#endif /* HAVE_SYSLOG_H */

#endif /* HAVE_FLOCK */

	return(0);
}

/****************************************************************************/
/* alarm_signal_handler()                                                   */
/*                                                                          */
/* Signal handler used when locks time out. It logs a message to syslog.    */
/****************************************************************************/
void alarm_signal_handler(int signum) {

	#ifdef HAVE_SYSLOG_H
	syslog(LOG_WARNING, "Waited %i seconds for a file lock, continuing without a lock", LOCK_TIMEOUT);
	#endif /* HAVE_SYSLOG_H */

	#ifdef HAVE_SIGNAL_H
	(void) (signal(SIGALRM, SIG_DFL)); /* Put the default signal back */
	#endif /* HAVE_SIGNAL_H */

	/* Don't call return, so that the blocked operation is aborted */
}

