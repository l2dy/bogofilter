/* $Id$ */

/*****************************************************************************

NAME:
   main.c -- detect spam and bogons presented on standard input.

AUTHOR:
   Eric S. Raymond <esr@thyrsus.com>

CONTRIBUTORS:
   Integrate patch by Zeph Hull and Clint Adams to present spamicity in
   X-Spam-Status header in -p (passthrough) mode.

******************************************************************************/

#include "common.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif

#include "bogoconfig.h"
#include "bogofilter.h"
#include "fgetsl.h"
#include "format.h"
#include "mime.h"
#include "paths.h"
#include "textblock.h"
#include "token.h"
#include "wordlists.h"
#include "xmalloc.h"
#ifdef	ENABLE_MEMDEBUG
#include "memdebug.h"
#endif

FILE  *fpo;

char msg_register[256];
char msg_bogofilter[256];
size_t msg_register_size = sizeof(msg_register);

const char *progname = "bogofilter";

/* Function Prototypes */

static FILE *output_setup(void);
void passthrough_setup(void);
void passthrough_cleanup(void);
static void write_log_message(void);
void write_message(rc_t status);

/* Function Definitions */

static void cleanup_exit(ex_t exitcode, int killfiles) {
    if (killfiles && outfname[0] != '\0') unlink(outfname);
    exit(exitcode);
}

/* check for non-empty blank line */
static bool is_blank_line(const char *line, size_t len)
{
    while (len > 0) {
	if (!isspace((unsigned char)*line) && *line != '\b')
	    return false;
	len -= 1;
	line++;
    }
    return true;
}

int main(int argc, char **argv) /*@globals errno,stderr,stdout@*/
{
    rc_t status;
    ex_t exitcode = EX_OK;

    progtype = build_progtype(progname, DB_TYPE);

    process_args_and_config_file(argc, argv, true);
    argc -= optind;
    argv += optind;

    /* open all wordlists */
    open_wordlists((run_type == RUN_NORMAL) ? DB_READ : DB_WRITE);

    fpo = output_setup();

    status = bogofilter(argc, argv);

    switch (status) {
    case RC_SPAM:	exitcode = EX_SPAM;	break;
    case RC_HAM:	exitcode = EX_HAM;	break;
    case RC_UNSURE:	exitcode = EX_UNSURE;	break;
    case RC_OK:		exitcode = EX_OK;	break;
    default:
	fprintf(dbgout, "Unexpected status code - %d\n", status);
	exit(EX_ERROR);
    }

    if (nonspam_exits_zero && exitcode != EX_ERROR)
	exitcode = EX_OK;

    close_wordlists(false);
    free_wordlists();

    if (logflag)
	write_log_message();

    /* cleanup storage */
    ds_cleanup();
    mime_cleanup();
    token_cleanup();
    xfree(directory);

#ifdef	ENABLE_MEMDEBUG
    memdisplay();
#endif

    exit(exitcode);
}

static int read_mem(char **out, void *in) {
    textdata_t **text = in;
    if ((*text)->next) {
	int s = (*text)->size;
	*out = (char *)(*text)->data;
	*text = (*text)->next;
	return s;
    }
    return 0;
}

static int read_seek(char **out, void *in) {
    static char buf[4096];
    FILE *inf = in;
    static int carry[2] = { -1, -1 }; /* carry over bytes */
    int s, i;
    char *b = buf;
    int cap = sizeof(buf);

    for (i = 0; i < (int)sizeof(carry) && carry[i] != -1 ; i++) {
	buf[i] = carry[i];
	carry[i] = -1;
    }
    b += i;
    cap -= i;

    s = xfgetsl(b, cap, inf, 1);
    if (s == EOF) {
       if (i) s = i;
    } else {
	s += i;
    }

    /* we must take care that on overlong lines, the \n doesn't appear
     * at the beginning of the buffer, so we pull two characters out and 
     * store them in the carry array */
    if (s && buf[s-1] != '\n') {
	int c = 2;
	if (c > s) c = s;
	s -= c;
	for (i = 0; i < c ; i++) {
	    carry[i] = (unsigned char)buf[s+i];
	}
    }

    *out = buf;
    return s;
}

typedef int (*readfunc_t)(char **, void *);

void write_message(rc_t status)
{
    ssize_t rd;
    readfunc_t rf = NULL;	/* assignment to quench warning */
    void *rfarg = 0;		/* assignment to quench warning */
    char *out;
    textdata_t *text;
    int seen_subj = 0;

    if (passthrough) {
	int hadlf = 1;
	int bogolen = strlen(spam_header_name);
	const char *subjstr = "Subject:";
	int subjlen = strlen(subjstr);
	/* initialize */
	switch (passmode) {
	    case PASS_MEM:
		rf = read_mem;
		text = textblock_head();
		rfarg = &text;
		break;
	    case PASS_SEEK:
		rf = read_seek;
		rfarg = fpin;
		rewind(rfarg);
		break;
	    default:
		bf_abort();
	}

	/* print headers */
	while ((rd = rf(&out, rfarg)) > 0)
	{
	    /* skip over spam_header ("X-Bogosity:") lines */
	    while (rd >= bogolen && memcmp(out, spam_header_name, bogolen) == 0) {
		while (((rd = rf(&out, rfarg)) > 0) && 
		       (out[0] == ' ' || out[0] == '\t') )
		    /* empty loop */ ;
	    }

	    /* detect end of headers */
	    if (is_eol(out, rd) ||
		is_blank_line(out, rd))		/* check for non-empty blank line */
		break;

	    /* rewrite "Subject: " line */
	    if (status == RC_SPAM &&
		rd >= subjlen && 
		spam_subject_tag != NULL &&
		strncasecmp(out, subjstr, subjlen) == 0) {
		(void) fprintf(fpo, "%.*s %s", subjlen, out, spam_subject_tag);
		if (out[subjlen] != ' ')
		    fputc(' ', fpo);
		(void) fwrite(out + subjlen, 1, rd - subjlen, fpo);
		seen_subj = 1;
		continue;
	    }

	    hadlf = (out[rd-1] == '\n');
	    (void) fwrite(out, 1, rd, fpo);
	    if (ferror(fpo)) cleanup_exit(2, 1);
	}

	if (!hadlf)
	    fputc('\n', fpo);
    }

    if (passthrough || verbose) {
	typedef char *formatter(char *buff, size_t size);
	formatter *fcn = terse ? format_terse : format_header;
	char buff[256];
	/* print spam-status at the end of the header
	 * then mark the beginning of the message body */
	fcn(buff, sizeof(buff));
	fputs (buff, fpo);
	fputs ("\n", fpo);
    }

    if (verbose || passthrough || Rtable) {
	verbose += passthrough;
	print_stats( stdout );
	verbose -= passthrough;
    }

    if (passthrough && !seen_subj &&
	status == RC_SPAM && spam_subject_tag != NULL) {
	(void) fprintf(fpo, "Subject: %s\n", spam_subject_tag);
    }

    if (passthrough) {
	int hadlf = 1;
	/* If the message terminated early (without body or blank
	 * line between header and body), enforce a blank line to
	 * prevent anything past us from choking. */
	(void)fputc('\n', fpo);

	/* print body */
	while ((rd = rf(&out, rfarg)) > 0)
	{
	    (void) fwrite(out, 1, rd, fpo);
	    hadlf = (out[rd-1] == '\n');
	    if (ferror(fpo)) cleanup_exit(2, 1);
	}

	if (!hadlf) fputc('\n', fpo);

	if (fflush(fpo) || ferror(fpo) || (fpo != stdout && fclose(fpo))) {
	    cleanup_exit(2, 1);
	}
    }
}

static void write_log_message(void)
{
#ifdef HAVE_SYSLOG_H
    openlog("bogofilter", LOG_PID, LOG_MAIL);

    format_log_header(msg_bogofilter, sizeof(msg_bogofilter));

    switch (run_type)
    {
    case RUN_NORMAL:
	syslog(LOG_INFO, "%s\n", msg_bogofilter);
	break;
    case RUN_UPDATE:
	syslog(LOG_INFO, "%s, %s\n", msg_bogofilter, msg_register);
	break;
    default:
	syslog(LOG_INFO, "%s", msg_register);
	break;
    }

    closelog();
#endif
}

static FILE *output_setup(void)
{
    FILE *fp;
    if (*outfname && passthrough) {
	if ((fp = fopen(outfname,"wt"))==NULL)
	{
	    fprintf(stderr,"Cannot open %s: %s\n",
		    outfname, strerror(errno));
	    exit(EX_ERROR);
	}
    } else {
	fp = stdout;
    }
    return fp;
}

void passthrough_setup()
{
    /* check if the input is seekable, if it is, we don't need to buffer
     * things in memory => configure passmode accordingly
     */

    if (!passthrough)
	return;

    passmode = PASS_MEM;

    if (passmode == PASS_MEM)
	textblock_init();

    if (DEBUG_GENERAL(2)) {
	const char *m;
	switch (passmode) {
	case PASS_MEM:  m = "cache in memory"; break;
	case PASS_SEEK: m = "rewind and reread file"; break;
	default:        m = "unknown"; break;
	}
	fprintf(fpo, "passthrough mode: %s\n", m);
    }
}

void passthrough_cleanup()
{
    if (!passthrough)
	return;

    switch(passmode) {
    case PASS_MEM:
	textblock_free();
	break;
    case PASS_SEEK: default:
	break;
    }
}

/* End */
