/* $Id$ */

/*****************************************************************************

NAME:
   bogoreader.c -- process input files

AUTHORS: (C) Copyright 2003 by
   David Relson <relson@osagesoftware.com>
   Matthias Andree <matthias.andree@gmx.de>

******************************************************************************/

#include "common.h"
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "buff.h"
#include "error.h"
#include "fgetsl.h"
#include "lexer.h"
#include "bogoreader.h"
#include "system.h"
#include "xmalloc.h"

static void (*fini)(void);
static int  argc;
static char **argv;
static char *filename;
static char namebuff[PATH_LEN+1];
static char dirname[PATH_LEN+1];

static FILE *yy_file;

static bool mail_first = true;  /* for the _next_mail functions */
static bool mailstore_first = true; /* for the _next_mailstore functions */
static bool emptyline = false; /* for mailbox /^From / match */

#ifdef	DUP_REF_RSLTS
static word_t *line_save = NULL;
#else
static bool    have_message = false;
#endif

/* Lexer-Reader Interface */

reader_more_t *reader_more;
reader_line_t *reader_getline;
reader_file_t *reader_filename;

/* Function Prototypes */

/* these functions check if there are more file names in bulk modes,
 * read-mail/mbox-from-stdin for uniformity */
static reader_more_t stdin_next_mailstore;
static reader_more_t b_stdin_next_mailstore;
static reader_more_t b_args_next_mailstore;

/* these functions check if there is more mail in a mailbox/maildir/...
 * to process, trivial mail_next_mail for uniformity */
static reader_more_t mail_next_mail;
static reader_more_t mailbox_next_mail;
static reader_more_t maildir_next_mail;
/* maildir is the mailbox format specified in
 * http://cr.yp.to/proto/maildir.html */

/* this function checks if there is more mail in a MH directory...
 * to process, trivial mail_next_mail for uniformity */
static reader_more_t mh_next_mail;

static reader_line_t simple_getline;	/* ignores /^From / */
static reader_line_t mailbox_getline;	/* minds   /^From / */

static reader_file_t get_filename;

static void maildir_init(const char *name);
static void maildir_fini(void);

static void mh_init(const char *name);
static void mh_fini(void);

typedef enum st_e { IS_DIR, IS_FILE, IS_ERR } st_t;

/* Function Definitions */

/* Checks if name is a directory.
 * Returns IS_DIR for directory, IS_FILE for other type, IS_ERR for error
 */
static st_t isdir(const char *name)
{
    struct stat stat_buf;
    if (stat(name, &stat_buf)) return IS_ERR;
    return (S_ISDIR(stat_buf.st_mode) != 0) ? IS_DIR : IS_FILE;
}

static void save_dirname(const char *name)
{
    size_t l = strlen(name);
    l = min(l, sizeof(dirname)-2);
    memcpy(dirname, name, l);
    if (dirname[l-1] == '/')
	dirname[l-1] = '\0';
}

static const char* const maildir_subs[]={ "/new", "/cur", NULL };
static const char *const *maildir_sub;
static DIR *reader_dir;

/* MA: Check if the given name points to a Maildir. We don't require the
 * /tmp directory for simplicity.
 * This function checks if dir, dir/new and dir/cur are all directories.
 * Returns IS_DIR for directory, IS_FILE for other type, IS_ERR for error
 */
static st_t ismaildir(const char *dir) {
    st_t r;
    size_t l;
    char *x;
    const char *const *y;
    const int maxlen = 4;

    r = isdir(dir);
    if (r != IS_DIR) return r;
    x = xmalloc((l = strlen(dir)) + maxlen /* append */ + 1 /* NUL */);
    memcpy(x, dir, l);
    for (y = maildir_subs; *y; y++) {
	strlcpy(x + l, *y, maxlen + 1);
	r = isdir(x);
	if (r != IS_DIR) {
	    free(x);
	    return r;
	}
    }
    free(x);
    return IS_DIR;
}


static void dummy_fini(void) { }

static reader_more_t *object_next_mailstore;
static reader_more_t *object_next_mail = NULL;

/* this is the 'nesting driver' for our input.
 * object := one out of { mail, mbox, maildir }
 * if we have a current object-specific handle, check that if we have
 * further input in the object first. if we don't, see if we have
 * further objects to process
 */
static bool reader__next_mail(void)
{
    for (;;) {
	/* check object-specific method */
	if (object_next_mail) {
	    if ((*object_next_mail)()) /* more mails in the object */
		return true;
	    object_next_mail = NULL;
	}

	/* ok, that one has been exhausted, try the next object */

	/* object_next_mailstore opens the object */
	if (!object_next_mailstore())
	    return NULL;

	/* ok, we have more objects, so check if the current object has
	 * input - loop.
	 */
    }
}

/* open object (Maildir, mbox file or file with a single mail) and set
 * _getline and _next_mail pointers dependent on what the obj is.
 *
 * - automatically detects maildir
 * - does not automatically distinguish between mbox and mail
 *   and takes mbox_mode instead
 */
static bool open_object(char *obj)
{
    filename = obj;
    if (fpin) {
	fclose(fpin);
	fpin = NULL;
    }
    switch (isdir(filename)) {
    case IS_FILE:
	if (DEBUG_READER(0))
	    fprintf(dbgout, "%s:%d - assuming %s is a %s\n", __FILE__, __LINE__, filename, mbox_mode ? "mbox" : "mail");
	fpin = fopen( filename, "r" );
	if (fpin == NULL) {
	    fprintf(stderr, "Can't open file '%s': %s\n", filename,
		    strerror(errno));
	    return false;
	}
	else {
	    emptyline = false;
	    mail_first = true;
	    reader_getline   = mbox_mode ? mailbox_getline   : simple_getline;
	    object_next_mail = mbox_mode ? mailbox_next_mail : mail_next_mail;
	    return true;
	}
    case IS_DIR:
	if (ismaildir(filename) == IS_DIR) {
	    /* MAILDIR */
	    maildir_init(filename);
	    reader_getline   = simple_getline;
	    object_next_mail = maildir_next_mail;
	    return true;
	} 
	else {
	    /* MH */
	    mh_init(filename);
	    reader_getline   = simple_getline;
	    object_next_mail = mh_next_mail;
	    return true;
	} 
	/* fallthrough to error */
    case IS_ERR:
	fprintf(stderr, "Can't identify type of object '%s'\n", filename);
    }
    return false;
}

/*** _next_mailstore functions ***********************************************/

/* this initializes for reading a single mail or a mbox from stdin */
static bool stdin_next_mailstore(void)
{
    bool val = mailstore_first;
    reader_getline = mbox_mode ? mailbox_getline : simple_getline;
    object_next_mail = mbox_mode ? mailbox_next_mail : mail_next_mail;
    mailstore_first = false;
    return val;
}

/* this reads file names from stdin and processes them according to
 * their type */
static bool b_stdin_next_mailstore(void)
{
    int len;
    filename = namebuff;

    if ((len = fgetsl(namebuff, sizeof(namebuff), stdin)) <= 0)
	return false;

    if (len > 0 && filename[len-1] == '\n')
	filename[len-1] = '\0';

    return open_object(filename);
}

/* this reads file names from the command line and processes them
 * according to their type */
static bool b_args_next_mailstore(void)
{
    if (!*argv) return false;
    filename = *(argv++);
    return open_object(filename);
}

/*** _next_mail functions ***********************************************/

/* trivial function, returns true on first run,
 * returns false on all subsequent runs */
static bool mail_next_mail(void)
{
    if (mail_first) {
	mail_first = false;
	return true;
    }
    return false;
}

/* always returns true on the first run
 * subsequent runs return true when a From line was encountered */
static bool mailbox_next_mail(void)
{
#ifndef	DUP_REF_RSLTS
    bool val = have_message;
#else
    bool val = line_save != NULL;
#endif
    val = val || mail_first;
    mail_first = false;
    return val;
}

/* iterates over files in a maildir */
static bool maildir_next_mail(void)
{
    struct dirent *dirent;

trynext: /* ugly but simple */
    if (reader_dir == NULL) {
	/* open next directory */
	char *x;
	size_t siz;

	if (*maildir_sub == NULL)
	    return false; /* IMPORTANT for termination */
	siz = strlen(dirname) + 4 + 1;
	x = xmalloc(siz);
	strlcpy(x, dirname, siz);
	strlcat(x, *(maildir_sub++), siz);
	reader_dir = opendir(x);
	if (!reader_dir) {
	    fprintf(stderr, "cannot open directory '%s': %s", x,
		    strerror(errno));
	}
	free(x);
    }

    while ((dirent = readdir(reader_dir)) != NULL) {
	/* skip dot files */
	if (dirent->d_name[0] == '.')
	    continue;

	/* there's no need to skip directories here, we can do that
	 * later */
	break;
    }

    if (dirent == NULL) {
	if (reader_dir)
	    closedir(reader_dir);
	reader_dir = NULL;
	goto trynext;
    }

    filename = namebuff;
    snprintf(namebuff, sizeof(namebuff), "%s%s/%s", dirname, *(maildir_sub-1),
	    dirent->d_name);

    if (fpin)
	fclose(fpin);
    fpin = fopen( filename, "r" );
    if (fpin == NULL) {
	fprintf(stderr, "Warning: can't open file '%s': %s\n", filename,
		strerror(errno));
	/* don't barf, the file may have been changed by another MUA,
	 * or a directory that just doesn't belong there, just skip it */
	goto trynext;
    }

    if (DEBUG_READER(0))
	fprintf(dbgout, "%s:%d - reading %s (%p)\n", __FILE__, __LINE__, filename, fpin);

    return true;
}

/* iterates over files in a MH directory */
static bool mh_next_mail(void)
{
    struct dirent *dirent;

    while (true) {
	if (reader_dir == NULL) {
	    reader_dir = opendir(dirname);
	    if (!reader_dir) {
		fprintf(stderr, "cannot open directory '%s': %s", dirname,
			strerror(errno));
	    }
	}

	while ((dirent = readdir(reader_dir)) != NULL) {
	    /* skip private files */
	    if (dirent->d_name[0] != '.' &&
		dirent->d_type == DT_REG)
		break;
	}

	if (dirent == NULL) {
	    if (reader_dir)
		closedir(reader_dir);
	    reader_dir = NULL;
	    return false;
	}

	filename = namebuff;
	snprintf(namebuff, sizeof(namebuff), "%s/%s", dirname, dirent->d_name);

	if (fpin)
	    fclose(fpin);
	fpin = fopen( filename, "r" );
	if (fpin == NULL) {
	    fprintf(stderr, "Warning: can't open file '%s': %s\n", filename,
		    strerror(errno));
	    /* don't barf, the file may have been changed by another MUA,
	     * or a directory that just doesn't belong there, just skip it */
	    continue;
	}

	if (DEBUG_READER(0))
	    fprintf(dbgout, "%s:%d - reading %s (%p)\n", __FILE__, __LINE__, filename, fpin);

	return true;
    }
}

/*** _getline functions ***********************************************/

/* reads from a mailbox, paying attention to ^From lines */
static int mailbox_getline(buff_t *buff)
{
    size_t used = buff->t.leng;
    byte *buf = buff->t.text + used;
    int count;

#ifndef	DUP_REF_RSLTS
    count = buff_fgetsl(buff, fpin);
    have_message = false;
#else
    if (!line_save) {
	count = buff_fgetsl(buff, fpin);
    }
    else {
	count = buff_add(buff, line_save);
	word_free(line_save);
	line_save = NULL;
	emptyline = false;
    }
#endif

    buf = buff->t.text + used;

    /* XXX FIXME: do we need to unescape the >From, >>From, >>>From, ... lines
     * by discarding the first ">"? */

    /* DR 08/25/03 - NO!!! */

    if (emptyline
	&& count >= 5
	&& memcmp("From ", buf, 5) == 0)
    {
#ifndef	DUP_REF_RSLTS
	have_message = true;
#else
	line_save = word_new(NULL, count);
	memcpy(line_save->text, buf, count);
#endif
	count = EOF;
    } else {
	if (buff->t.leng < buff->size)		/* for easier debugging - removable */
	    Z(buff->t.text[buff->t.leng]);	/* for easier debugging - removable */
    }

    emptyline = (count == 1 && *buf == '\n');

    return count;
}

/* reads a whole file as a mail, no ^From detection */
static int simple_getline(buff_t *buff)
{
    size_t used = buff->t.leng;
    byte *buf = buff->t.text + used;
    int count;

#ifndef	DUP_REF_RSLTS
    count = buff_fgetsl(buff, fpin);
    have_message = false;
#else
    if (!line_save) {
	count = buff_fgetsl(buff, fpin);
    }
    else {
	count = buff_add(buff, line_save);
	word_free(line_save);
	line_save = NULL;
    }
#endif

    buf = buff->t.text + used;

    if (buff->t.leng < buff->size)	/* for easier debugging - removable */
	Z(buff->t.text[buff->t.leng]);	/* for easier debugging - removable */

    return count;
}

/* maildir specific functions */

static void maildir_fini(void)
{
    if (reader_dir)
	closedir(reader_dir);
    reader_dir = NULL;
    return;
}

/* initialize iterators for Maildir subdirectories, 
 * cur and new. */
static void maildir_init(const char *name)
{
    maildir_sub = maildir_subs;
    reader_dir = NULL;
    fini = maildir_fini;

    save_dirname(name);

    return;
}

/* MH specific functions */

static void mh_fini(void)
{
    if (reader_dir)
	closedir(reader_dir);
    reader_dir = NULL;
    return;
}

/* initialize iterators for MH subdirectories, 
 * cur and new. */
static void mh_init(const char *name)
{
    reader_dir = NULL;
    fini = mh_fini;

    save_dirname(name);

    return;
}

/* returns current file name */
static const char *get_filename(void)
{
    return filename;
}

/* global reader initialization, exported */
void bogoreader_init(int _argc, char **_argv)
{
    mailstore_first = mail_first = true;
    reader_getline = mailbox_getline;
    reader_more = reader__next_mail;
    fini = dummy_fini;
    if (run_type & (REG_SPAM|REG_GOOD|UNREG_SPAM|UNREG_GOOD))
	mbox_mode = true;

    switch (bulk_mode) {
    case B_NORMAL:		/* read mail (mbox) from stdin */
	yy_file = fpin;
	object_next_mailstore = stdin_next_mailstore;
	break;
    case B_STDIN:		/* '-b' - streaming (stdin) mode */
	object_next_mailstore = b_stdin_next_mailstore;
	break;
    case B_CMDLINE:		 /* '-B' - command line mode */
	argc = _argc;
	argv = _argv;
	object_next_mailstore = b_args_next_mailstore;
	break;
    default:
	fprintf(stderr, "Unknown bulk_mode = %d\n", (int) bulk_mode);
	abort();
	break;
    }
    reader_filename = get_filename;
}

/* global cleanup, exported */
void bogoreader_fini(void)
{
    if (fpin && fpin != stdin)
	fclose(fpin);
    fini();
}


