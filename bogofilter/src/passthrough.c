/* $Id$ */

/*****************************************************************************

NAME:
   passthrough.c -- handle writing out message in passthrough ('-p') mode.

******************************************************************************/

#include "common.h"

#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "passthrough.h"
#include "bogofilter.h"
#include "fgetsl.h"
#include "format.h"
#include "textblock.h"
#include "xmalloc.h"

#include "lexer.h" /* need have_body */

FILE  *fpo;

char msg_register[256];
char msg_bogofilter[256];
size_t msg_register_size = sizeof(msg_register);

/* Function Definitions */

static bool is_eol(const char *buf, size_t len)
{
    bool ans = ((len == 1 && memcmp(buf, NL, 1) == 0) ||
		(len == 2 && memcmp(buf, CRLF, 2) == 0));
    return ans;
}
 
static void cleanup_exit(ex_t exitcode, int killfiles) {
    if (killfiles && outfname[0] != '\0') unlink(outfname);
    exit(exitcode);
}

#define	ISSPACE(ch)	(isspace(ch) || (ch == '\b'))

/* check for non-empty blank line */
static bool is_blank_line(const char *line, size_t len)
{
    if (have_body)
	return len == 0;

    while (len-- > 0) {
	byte ch = *line++;
	if (!ISSPACE(ch))
	    return false;
    }

    return true;
}

/** check if the line we're looking at is a header/body delimiter */
static bool is_hb_delim(const char *line, size_t len, bool strict_body)
{
    if (strict_body)
	return len == 0;

    return is_blank_line(line, len);
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
		abort();
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
		is_hb_delim(out, rd, have_body))
		/* check for non-empty blank line */
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

    if (passthrough || verbose || terse) {
	typedef char *formatter(char *buff, size_t size);
	formatter *fcn = terse ? format_terse : format_header;
	char buff[256];
	/* print spam-status at the end of the header
	 * then mark the beginning of the message body */
	(*fcn)(buff, sizeof(buff));
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

void write_log_message(rc_t status)
{
#ifdef HAVE_SYSLOG_H
    format_log_header(msg_bogofilter, sizeof(msg_bogofilter));

    if (run_type == RUN_NORMAL ||
	(run_type == RUN_UPDATE && status == RC_UNSURE))
	syslog(LOG_INFO, "%s\n", msg_bogofilter);
    else if (run_type == RUN_UPDATE)
	syslog(LOG_INFO, "%s, %s\n", msg_bogofilter, msg_register);
    else
	syslog(LOG_INFO, "%s", msg_register);

    closelog();
#endif
}

void output_setup(void)
{
    if (*outfname && passthrough) {
	if ((fpo = fopen(outfname,"wt"))==NULL)
	{
	    fprintf(stderr,"Cannot open %s: %s\n",
		    outfname, strerror(errno));
	    exit(EX_ERROR);
	}
    } else {
	fpo = stdout;
    }
    return;
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
