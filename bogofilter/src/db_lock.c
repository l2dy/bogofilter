/* Lock handler to detect application crashes
 * (C) 2004 Matthias Andree, GNU GPL v2
 * with optimization ideas by Pavel Kankovsky */

/* Lock file layout:
 * the lock file has a list of cells, which can be either 0 or 1.
 * 0 means: slot is free
 * 1 means: slot in use
 *   1 with fcntl lock: process running
 *   1 without lock: process quit prematurely, recovery needed
 * see http://article.gmane.org/gmane.mail.bogofilter.devel/3240
 *     http://article.gmane.org/gmane.mail.bogofilter.devel/3260
 *     http://article.gmane.org/gmane.mail.bogofilter.devel/3270
 */

#include "db_lock.h"
#include "debug.h"
#include "error.h"
#include "globals.h"
#include "mxcat.h"
#include "system.h"
#include "xstrdup.h"
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>

#if HAVE_DECL_O_DSYNC
static const int syncflag = O_DSYNC;
#elif HAVE_DECL_O_SYNC
static const int syncflag = O_SYNC;
#elif HAVE_DECL_O_FSYNC
static const int syncflag = O_FSYNC;
#endif

static const int chk_intval = 30;	/* check interval in seconds */
static const char aprt[] = DIRSEP_S "process-table";
static const off_t cellsize = 1;
static off_t lockpos;			/* lock cell offset */
static int lockfd = -1;			/* lock file descriptor */

static const char cell_inuse = '1';
static const char cell_free = '0';

static const char *lockdir; /* pointer to lock directory */

static struct sigaction oldact; /* save area */

/* the lock file descriptor must be long-lived because fcntl() or
 * lockf() will immediately release all locks held on a file once we
 * call close() for the file for the first time no matter how many file
 * descriptors to the file we still hold, so we cannot use
 * open-check-close cycles but need to keep the
 * descriptor open until will let go of the locks. */

typedef char cell_t;

/* 1 if locked, 0 if unlocked, negative if error */
static int check_celllock(int fd, off_t offset) {
    struct flock fl;
    int r;

    fl.l_type = F_RDLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = offset;
    fl.l_len = cellsize;
    r = fcntl(fd, F_GETLK, &fl);
    if (r) {
	if (DEBUG_DATABASE(2))
	    fprintf(dbgout, "check_celllock(fd=%d, offset=%ld) failed: %s\n",
		    fd, (long)offset, strerror(errno));
	return -1;
    }
    if (DEBUG_DATABASE(2))
	fprintf(dbgout, "check_celllock(fd=%d, offset=%ld) = %s\n",
		fd, (long)offset, fl.l_type == F_UNLCK ? "unlocked" : "locked");
    return fl.l_type == F_UNLCK ? 0 : 1;
}

/* fast lock function, uses F_SETLK so does not wait */
static int set_celllock(int fd, off_t offset, int locktype) {
    struct flock fl;
    int r;

    fl.l_type = locktype;
    fl.l_whence = SEEK_SET;
    fl.l_start = offset;
    fl.l_len = cellsize;
    r = fcntl(fd, F_SETLK, &fl);
    if (DEBUG_DATABASE(2))
	fprintf(dbgout, "set_celllock(fd=%d, offset=%ld, type=%d) = %d%s%s\n",
		fd, (long)offset, locktype, r, r < 0 ? ", " : "", r < 0 ? strerror(errno) : "");
    return r;
}

/* returns descriptor of open lock file for success,
 * -1 for error */
static int create_lockfile(const char *fn, int modes) {
    lockfd = open(fn, modes|O_CREAT|O_EXCL, 0664); /* umask will decide about group writability */
    if (lockfd >= 0) {
	char b[1024];	/* XXX FIXME: make lock size configurable */

	memset(b, (unsigned char)cell_free, sizeof(b)); /* XXX FIXME: only works for char */
	if (sizeof(b) != write(lockfd, b, sizeof(b))
		|| fsync(lockfd)) {
	    close(lockfd);
	    unlink(fn);
	    return -1;
	}
    }
    return lockfd;
}

/* returns 0 for success, -1 for error
 * this will do nothing and flag success if the file is open already */
static int open_lockfile(const char *bogodir) {
    char *fn;
    int modes = O_RDWR|syncflag;

    if (lockfd >= 0) return 0;
    fn = mxcat(bogodir, aprt, NULL);

    lockfd = open(fn, modes);
    if (lockfd < 0 && errno == ENOENT)
	lockfd = create_lockfile(fn, modes);
    if (lockfd < 0) {
	print_error(__FILE__, __LINE__, "open_lockfile: open(%s): %s\n",
		fn, strerror(errno));
    } else {
	if (DEBUG_DATABASE(1)) {
	    fprintf(dbgout, "open_lockfile: open(%s) succeeded, fd #%d\n", fn, lockfd);
	}
    }

    free(fn);
    return (lockfd < 0) ? -1 : 0;
}

static int close_lockfile(void) {
    int r = 0;

    if (lockfd >= 0) {
	if (DEBUG_DATABASE(1))
	    fprintf(dbgout, "close_lockfile\n");
	r = close(lockfd);
	if (r) {
	    int e = errno;
	    print_error(__FILE__, __LINE__, "close_lockfile: close(%d) failed: %s",
		    lockfd, strerror(errno));
	    errno = e;
	}
	lockfd = -1;
    }
    return r;
}

/*  0 - no zombies
 *  1 - zombies
 *  2 - file not found
 * -1 - other error */
static int check_zombies(const char *bogodir) {
    ssize_t r;
    off_t pos;
    cell_t cell;

    if (open_lockfile(bogodir)) {
	return errno == ENOENT ? 2 : -1;
    }

    if (lseek(lockfd, 0, SEEK_SET) < 0)
	return -1;

    for (;;) {
	pos = lseek(lockfd, 0, SEEK_CUR);
	r = read(lockfd, &cell, sizeof(cell));
	if (r != sizeof(cell)) break;
	if (cell == cell_inuse && 1 != check_celllock(lockfd, pos)) {
	    return 1;
	}
    }

    return r == 0 ? 0 : -1;
}

static void check_lock(int unused) {
    (void)unused;

    if (0 != check_zombies(lockdir)) {
	print_error(__FILE__, __LINE__, "bogofilter or related application has crashed or directory damaged, aborting.");
	_exit(EX_ERROR);	/* use _exit, not exit, to avoid running the atexit handler that might deadlock */
    }
    alarm(chk_intval);
}

/* returns 0 for success, -1 for error */
static int init_sig(void) {
    struct sigaction sa;
    sigset_t ss;

    sigemptyset(&ss);

    sa.sa_handler = check_lock;
    sa.sa_mask = ss;
    sa.sa_flags = 0;
    if (sigaction(SIGALRM, &sa, &oldact)) return -1;

    alarm(chk_intval);
    return 0;
}

static int shut_sig(void) {
    alarm(0);
    return sigaction(SIGALRM, &oldact, NULL);
}

/*
 * lock a cell and returns
 *  0 for success,
 * -2 if zombie was found or
 * -1 in case of trouble
 */
int set_lock(const char *bogohome) {
    cell_t cell;
    ssize_t r;

    lockdir = xstrdup(bogohome);

    if (open_lockfile(lockdir)) {
	return errno == ENOENT ? -2 : -1;
    }

    if (lseek(lockfd, 0, SEEK_SET) < 0)
	return -1;

    for (;;) {
	lockpos = lseek(lockfd, 0, SEEK_CUR);
	r = read(lockfd, &cell, sizeof(cell));
	if (r != sizeof(cell))
	    return -1; /* XXX FIXME: retry? */
	if (cell == cell_free && 0 == set_celllock(lockfd, lockpos, F_WRLCK)) {
	    if (lseek(lockfd, lockpos, SEEK_SET) >= 0) {
		r = read(lockfd, &cell, sizeof(cell));
		if (r != sizeof(cell) || cell != cell_free) {
		    /* found fresh zombie */
		    set_celllock(lockfd, lockpos, F_UNLCK);
		    return -2;
		}
		if (lseek(lockfd, lockpos, SEEK_SET) < 0)
		    return -1;
		if (cellsize != write(lockfd, &cell_inuse, cellsize))
		    return -1;
#if 0
/* disabled for now, O_{D,,F}SYNC should handle this for us */
#ifdef HAVE_FDATASYNC
		if (fdatasync(lockfd))
		    return -1;
#else
		if (fsync(lockfd))
		    return -1;
#endif
#endif

		init_sig();
		return 0;
	    }
	}
    }
}

/* returns 0 for success */
int clear_lock(void) {
    shut_sig();
    if (lseek(lockfd, lockpos, SEEK_SET) < 0)
	return -1;
    if (cellsize != write(lockfd, &cell_free, cellsize))
	return -1;
    if (set_celllock(lockfd, lockpos, F_UNLCK))
	return -1;
    if (close_lockfile())
	return -1;
    return 0;
}

int needs_recovery(const char *bogodir) {
    return !!check_zombies(bogodir);
}


int clear_lockfile(const char *bogodir) {
    char *fn;

    fn = mxcat(bogodir, aprt, NULL);
    if (DEBUG_DATABASE(1))
	fprintf(dbgout, "clear_lockfile(%s)\n", fn);
    if (unlink(fn)) {
	if (errno == ENOENT)
	    return 0;
	return -1;
    }
    return 0;
}
