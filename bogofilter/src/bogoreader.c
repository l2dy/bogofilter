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

static bool first = true;
static bool emptyline = false;

#ifdef	DUP_REF_RSLTS
static word_t *line_save = NULL;
#else
static bool    have_message = false;
#endif

/* Function Prototypes */

static lexer_more_t normal_more;
static lexer_more_t cmdline_more;
static lexer_more_t stdin_more;
static lexer_more_t maildir_more;

static lexer_line_t mailbox_getline;
static lexer_file_t mailbox_filename;

/* maildir is the mailbox format specified in
 * http://cr.yp.to/proto/maildir.html */
static lexer_line_t maildir_getline;
static lexer_file_t maildir_filename;

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

void bogoreader_init(int _argc, char **_argv)
{
    first = true;
    lexer_getline = mailbox_getline;
    fini = dummy_fini;

    switch (bulk_mode) {
    case B_NORMAL:
	yy_file = fpin;
	lexer_more = normal_more;
	break;
    case B_STDIN:		/* '-b' - streaming (stdin) mode */
	lexer_more = stdin_more;
	break;
    case B_CMDLINE:		 /* '-B' - command line mode */
	argc = _argc;
	argv = _argv;
	lexer_more = cmdline_more;
	break;
    default:
	fprintf(stderr, "Unknown bulk_mode = %d\n", (int) bulk_mode);
	abort();
	break;
    }
    lexer_filename = mailbox_filename;
}

void bogoreader_fini(void)
{
    fini();
}

static bool normal_more(void)
{
    bool mailbox = (mbox_mode ||			/* '-M' */
		    ((run_type & (REG_SPAM |		/* '-s' */
				  UNREG_SPAM |		/* '-S' */
				  REG_GOOD |		/* '-n' */
				  UNREG_GOOD)) != 0));	/* '-N' */
#ifndef	DUP_REF_RSLTS
    bool val = first || (mailbox && have_message);
#else
    bool val = first || (mailbox && line_save != NULL);
#endif
    first = false;
    return val;
}

static bool cmdline_more(void)
{
    bool val = *argv != NULL;

    if (val) {
	filename = *argv++;
	if (fpin)
	    fclose(fpin);
	if (isdir(filename) == 0) {
	    if (DEBUG_READER(0))
		fprintf(dbgout, "%s:%d - assuming %s is a file\n", __FILE__, __LINE__, filename);
	    fpin = fopen( filename, "r" );
	    if (fpin == NULL) {
		fprintf(stderr, "Can't open file '%s': %s\n", filename,
			strerror(errno));
		return false;
	    }
	    emptyline = false;
	} else if (ismaildir(filename) == 1) {
	    if (DEBUG_READER(0))
		fprintf(dbgout, "%s:%d - assuming %s is a maildir\n", __FILE__, __LINE__, filename);
	    maildir_init(filename);
	    lexer_more = maildir_more;
	    lexer_more();
	} else {
	    fprintf(stderr, "Can't identify type of object '%s'\n", filename);
	    return false;
	}
    }
    return val;
}

static bool stdin_more(void)
{
    int len;
    filename = namebuff;

    if ((len = fgetsl(namebuff, sizeof(namebuff), stdin)) <= 0)
	return false;

    if (len > 0 && filename[len-1] == '\n')
	filename[len-1] = '\0';

    if (fpin)
	fclose(fpin);
    fpin = fopen( filename, "r" );
    if (fpin == NULL) {
	fprintf(stderr, "Can't open file '%s': %s\n", filename,
		strerror(errno));
	return false;
    }

    emptyline = false;

    return true;
}

char *mailbox_filename(void)
{
    return filename;
}

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
    }
    else {
	if (buff->t.leng < buff->size)		/* for easier debugging - removable */
	    Z(buff->t.text[buff->t.leng]);	/* for easier debugging - removable */
    }

    emptyline = (count == 1 && *buf == '\n');

    return count;
}

static void maildir_fini(void)
{
    if (maildir_dir)
	closedir(maildir_dir);
    if (fpin)
	fclose(fpin);
    maildir_dir = NULL;
    return;
}

static void maildir_init(const char *name)
{
    lexer_more = maildir_more;
    lexer_filename = maildir_filename;
    lexer_getline  = maildir_getline;
    maildir_sub = maildir_subs;
    maildir_dir = NULL;
    fini = maildir_fini;

    strlcpy(dirname, name, sizeof(dirname));

    return;
}

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

char *maildir_filename(void)
{
    return filename;
}
