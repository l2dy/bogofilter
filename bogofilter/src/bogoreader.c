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

static bool first = true;  /* for the _more functions */
static bool nfirst = true; /* for the _next functions */
static bool emptyline = false; /* for mailbox /^From / match */

#ifdef	DUP_REF_RSLTS
static word_t *line_save = NULL;
#else
static bool    have_message = false;
#endif

/* Function Prototypes */

/* these functions check if there are more file names in bulk modes,
 * read-mail/mbox-from-stdin for uniformity */
static lexer_more_t stdin_next;
static lexer_more_t b_stdin_next;
static lexer_more_t b_args_next;

/* these functions check if there is more mail in a mailbox/maildir/...
 * to process, trivial mail_more for uniformity */
static lexer_more_t mail_more;
static lexer_more_t mailbox_more;
static lexer_more_t maildir_more;
/* maildir is the mailbox format specified in
 * http://cr.yp.to/proto/maildir.html */

static lexer_line_t mailbox_getline; /* minds   /^From / */
static lexer_line_t maildir_getline; /* ignores /^From / */

static lexer_file_t get_filename;

static void maildir_init(const char *name);
static void maildir_fini(void);

/* Function Definitions */

/* Checks if name is a directory.
 * Returns 1 for directory, 0 for other type, -1 for error */
static int isdir(const char *name)
{
    struct stat stat_buf;
    if (stat(name, &stat_buf)) return -1;
    return S_ISDIR(stat_buf.st_mode);
}

static const char* const maildir_subs[]={ "/new", "/cur", NULL };
static const char *const *maildir_sub;
static DIR *maildir_dir;

/* MA: Check if the given name points to a Maildir. We don't require the
 * /tmp directory for simplicity.
 * This function checks if dir, dir/new and dir/cur are all directories.
 * Returns 1 for "yes", 0 for "no" or -1 for "error"
 */
static int ismaildir(const char *dir) {
    int r;
    size_t l;
    char *x;
    const char *const *y;
    const int maxlen = 4;

    r = isdir(dir);
    if (r != 1) return r;
    x = xmalloc((l = strlen(dir)) + maxlen /* append */ + 1 /* NUL */);
    memcpy(x, dir, l);
    for (y = maildir_subs; *y; y++) {
	strlcpy(x + l, *y, maxlen + 1);
	r = isdir(x);
	if (r != 1) {
	    free(x);
	    return r;
	}
    }
    free(x);
    return 1;
}

static void dummy_fini(void) { }

static lexer_more_t *object_next;
static lexer_more_t *object_more = NULL; /* for universal_more */

/* this is the 'nesting driver' for our input.
 * object := one out of { mail, mbox, maildir }
 * if we have a current object-specific handle, check that if we have
 * further input in the object first. if we don't, see if we have
 * further objects to process
 */
static bool lexer__more(void)
{
    for (;;) {
	/* check object-specific method */
	if (object_more) {
	    if ((*object_more)()) /* more mails in the object */
		return true;
	    object_more = NULL;
	}

	/* ok, that one has been exhausted, try the next object */

	/* object_next opens the object */
	if (!object_next())
	    return NULL;

	/* ok, we have more objects, so check if the current object has
	 * input - loop.
	 */
    }
}

/* open object (Maildir, mbox file or file with a single mail) and set
 * _getline and _more pointers dependent on what the obj is.
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
    if (ismaildir(filename) == 1) {
	/* MAILDIR */
	lexer_getline = maildir_getline;
	object_more = maildir_more;
	maildir_init(filename);
	return true;
    } else if (isdir(filename) == 1) {
	fprintf(stderr, "Can't identify type of object '%s'\n", filename);
	return false;
    } else {
	if (DEBUG_READER(0))
	    fprintf(dbgout, "%s:%d - assuming %s is a %s\n", __FILE__, __LINE__, filename, mbox_mode ? "mbox" : "mail");
	fpin = fopen( filename, "r" );
	if (fpin == NULL) {
	    fprintf(stderr, "Can't open file '%s': %s\n", filename,
		    strerror(errno));
	    return false;
	}
	emptyline = false;
	lexer_getline = mbox_mode ? mailbox_getline : maildir_getline;
	object_more = mbox_mode ? mailbox_more : mail_more;
	first = true;
	return true;
    }
}

/*** _next functions ***********************************************/

/* this initializes for reading a single mail or a mbox from stdin */
static bool stdin_next(void)
{
    bool val = nfirst;
    lexer_getline = mbox_mode ? mailbox_getline : maildir_getline;
    object_more = mbox_mode ? mailbox_more : mail_more;
    nfirst = false;
    return val;
}

/* this reads file names from stdin and processes them according to
 * their type */
static bool b_stdin_next(void)
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
static bool b_args_next(void)
{
    if (!*argv) return false;
    filename = *(argv++);
    return open_object(filename);
}

/*** _more functions ***********************************************/

/* trivial function, returns true on first run,
 * returns false on all subsequent runs */
static bool mail_more(void)
{
    if (first) {
	first = false;
	return true;
    }
    return false;
}

/* always returns true on the first run
 * subsequent runs return true when a From line was encountered */
static bool mailbox_more(void)
{
#ifndef	DUP_REF_RSLTS
    bool val = have_message;
#else
    bool val = line_save != NULL;
#endif
    val = val || first;
    first = false;
    return val;
}

/* iterates over files in a maildir */
static bool maildir_more(void)
{
    struct dirent *dirent;

trynext: /* ugly but simple */
    if (maildir_dir == NULL) {
	/* open next directory */
	char *x;
	size_t siz;

	if (*maildir_sub == NULL)
	    return false; /* IMPORTANT for termination */
	siz = strlen(dirname) + 4 + 1;
	x = xmalloc(siz);
	strlcpy(x, dirname, siz);
	strlcat(x, *(maildir_sub++), siz);
	maildir_dir = opendir(x);
	if (!maildir_dir) {
	    fprintf(stderr, "cannot open directory '%s': %s", x,
		    strerror(errno));
	}
	free(x);
    }

    while ((dirent = readdir(maildir_dir)) != NULL) {
	/* skip dot files */
	if (dirent->d_name[0] == '.')
	    continue;

	/* there's no need to skip directories here, we can do that
	 * later */
	break;
    }

    if (dirent == NULL) {
	if (maildir_dir)
	    closedir(maildir_dir);
	maildir_dir = NULL;
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
static int maildir_getline(buff_t *buff)
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
    if (maildir_dir)
	closedir(maildir_dir);
    maildir_dir = NULL;
    return;
}

/* initialize iterators for Maildir subdirectories, 
 * cur and new. */
static void maildir_init(const char *name)
{
    maildir_sub = maildir_subs;
    maildir_dir = NULL;
    fini = maildir_fini;

    strlcpy(dirname, name, sizeof(dirname));

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
    nfirst = first = true;
    lexer_getline = mailbox_getline;
    lexer_more = lexer__more;
    fini = dummy_fini;
    if (run_type & (REG_SPAM|REG_GOOD|UNREG_SPAM|UNREG_GOOD))
	mbox_mode = true;

    switch (bulk_mode) {
    case B_NORMAL:		/* read mail (mbox) from stdin */
	yy_file = fpin;
	object_next = stdin_next;
	break;
    case B_STDIN:		/* '-b' - streaming (stdin) mode */
	object_next = b_stdin_next;
	break;
    case B_CMDLINE:		 /* '-B' - command line mode */
	argc = _argc;
	argv = _argv;
	object_next = b_args_next;
	break;
    default:
	fprintf(stderr, "Unknown bulk_mode = %d\n", (int) bulk_mode);
	abort();
	break;
    }
    lexer_filename = get_filename;
}

/* global cleanup, exported */
void bogoreader_fini(void)
{
    if (fpin && fpin != stdin)
	fclose(fpin);
    fini();
}


