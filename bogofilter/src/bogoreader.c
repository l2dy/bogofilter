/* $Id$ */

/*****************************************************************************

NAME:
   bogoreader.c -- process input files

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#include "common.h"
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>

#include "buff.h"
#include "error.h"
#include "fgetsl.h"
#include "lexer.h"
#include "bogoreader.h"

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

static lexer_line_t maildir_getline;
static lexer_file_t maildir_filename;

static void maildir_init(const char *name);
static void maildir_fini(void);

/* Function Definitions */

static bool isdir(const char *name)
{
    struct stat stat_buf;
    stat(name, &stat_buf);
    return S_ISDIR(stat_buf.st_mode);
}

void bogoreader_init(int _argc, char **_argv)
{
    first = true;
    lexer_getline = mailbox_getline;

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
    /* Is this really a NOP, or is something missing? */
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
	if (!isdir(filename)) {
	    fpin = fopen( filename, "r" );
	    if (fpin == NULL) {
		fprintf(stderr, "Can't read file '%s'\n", filename);
		return false;
	    }
	    emptyline = false;
	}
	else {
	    maildir_init(filename);
	    return maildir_more();
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
	fprintf(stderr, "Can't read file '%s'\n", filename);
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

static DIR *dir;

static void maildir_init(const char *name)
{
    dir = opendir(name);

    if (dir == NULL) {
	perror(name);
	exit(EX_ERROR);
    }

    lexer_more = maildir_more;
    lexer_filename = maildir_filename;
    lexer_getline  = maildir_getline;

    strlcpy(dirname, name, sizeof(dirname));

    return;
}

static void maildir_fini(void)
{
    closedir(dir);
    dir = NULL;
    return;
}

static bool maildir_more(void)
{
    struct dirent *dirent;

    while ((dirent = readdir(dir)) != NULL) {
	if (dirent->d_type != DT_DIR &&
	    *dirent->d_name != '.')
	    break;
    }

    if (dirent == NULL) {
	maildir_fini();
	return false;
    }

    filename = namebuff;
    snprintf(namebuff, sizeof(namebuff), "%s/%s", dirname, dirent->d_name);

    if (fpin)
	fclose(fpin);
    fpin = fopen( filename, "r" );
    if (fpin == NULL) {
	fprintf(stderr, "Can't read file '%s'\n", filename);
	return false;
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
