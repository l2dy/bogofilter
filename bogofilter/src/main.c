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

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include <config.h>
#include "common.h"

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif

#include "bogoconfig.h"
#include "bogofilter.h"
#include "charset.h"
#include "collect.h"
#include "datastore.h"
#include "fgetsl.h"
#include "format.h"
#include "lexer.h"
#include "mime.h"
#include "msgcounts.h"
#include "register.h"
#include "rstats.h"
#include "textblock.h"
#include "token.h"
#include "wordlists.h"
#include "xmalloc.h"
#ifdef	ENABLE_MEMDEBUG
#include "memdebug.h"
#endif

#define	NL	"\n"
#define	CRLF	"\r\n"

extern int Rtable;

FILE  *fpo;

char msg_register[256];
char msg_bogofilter[256];
size_t msg_register_size = sizeof(msg_register);

const char *progname = "bogofilter";

/* Function Prototypes */

static FILE *output_setup(void);
static void passthrough_setup(void);
static void passthrough_cleanup(void);
static void write_log_message(void);
static void write_message(FILE *fp, rc_t status);

/* Function Definitions */

static void cleanup_exit(int exitcode, int killfiles) {
    if (killfiles && outfname[0] != '\0') unlink(outfname);
    exit(exitcode);
}

static rc_t classify(void);
static void initialize(FILE *fp);

static void initialize(FILE *fp)
{
    init_charset_table(charset_default, true);
    mime_reset();
    token_init();
    collect_reset();
    lexer_v3_init(fp);
}

typedef rc_t (*arg_foreach_t)(void);

static int arg_foreach(arg_foreach_t hook, int argc, char **argv)
{
    int  exitcode = 0;
    bool error = false;
    bool done = false;
    int  i = 0;

    while (!done) {
	rc_t status = RC_MORE;
	char *filename = NULL;
	char buff[PATH_LEN+1];

	switch (bulk_mode) {
	case B_NORMAL:
	    break;
	case B_STDIN:		/* '-b' - streaming (stdin) mode */
	{
	    int len;
	    filename = buff;
	    if ((len = fgetsl(buff, sizeof(buff), stdin)) <= 0) {
		done = true;
		continue;
	    }
	    if (len > 0 && filename[len-1] == '\n')
		filename[len-1] = '\0';
	    break;
	}
	case B_CMDLINE: /* '-B' - command line mode */
	    if (i < argc && !error) {
		filename = argv[i++];
	    } else {
		done = true;
		continue;
	    }
	    break;
	default:
	    fprintf(stderr, "Unknown bulk_mode = %d\n", (int) bulk_mode);
	    abort();
	    break;
	}
	if (bulk_mode != B_NORMAL) {
	    if (fpin)
		fclose(fpin);
	    fpin = fopen( filename, "r" );
	    if (fpin == NULL) {
		error = true;
		fprintf(stderr, "Can't read file '%s'\n", filename);
		continue;
	    }
	    initialize(fpin);
	    if (verbose || passthrough)
		fprintf(fpo, "%s ", filename ); 
	}

	status = hook();

	exitcode = !error ? 0 : 1;

	if (bulk_mode == B_NORMAL) {
	    if (run_register)
		done = true;
	    else {
		if (status != RC_MORE) {
		    exitcode = (status == RC_SPAM) ? 0 : 1;
		    if (nonspam_exits_zero && exitcode == 1)
			exitcode = 0;
		    done = true;
		}
	    }
	}
    }
    return exitcode;
}

/* check for non-empty blank line */
static bool is_blank_line(const char *line, size_t len)
{
    while (len > 0) {
	if (! isspace(*line) && *line != '\b' )
	    return false;
	len -= 1;
	line++;
    }
    return true;
}

int main(int argc, char **argv) /*@globals errno,stderr,stdout@*/
{
    int   exitcode;

    process_args_and_config_file(argc, argv, true);
    argc -= optind;
    argv += optind;

    /* open all wordlists */
    open_wordlists((run_type == RUN_NORMAL) ? DB_READ : DB_WRITE);

    fpo = output_setup();

    initialize(NULL);

    if (run_classify || passthrough)
	exitcode = arg_foreach(classify, argc, argv);
    else
	exitcode = arg_foreach(register_messages, argc, argv);

    close_wordlists(false);
    free_wordlists();

    if (logflag)
	write_log_message();

    /* cleanup storage */
    db_cleanup();
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

static rc_t classify()
{
    rc_t status = RC_MORE;

    do {
	passthrough_setup();
	init_msg_counts();
	token_init();
	init_charset_table(charset_default, true);
	
	status = bogofilter();
	write_message(fpo, status);
	
	rstats_cleanup();
	passthrough_cleanup();
    } while (status == RC_MORE);

    return status;
}

static void write_message(FILE *fp, rc_t status)
{
    ssize_t rd = 0;	/* assignment to quench warning */
    readfunc_t rf = 0;	/* assignment to quench warning */
    void *rfarg = 0;	/* assignment to quench warning */
    char *out;
    textdata_t *text;

    if (passthrough) {
	int hadlf = 1;
	int bogolen = strlen(spam_header_name);
	int subjlen = strlen("Subject:");
	/* initialize */
	switch (passmode) {
	    case PASS_MEM:
		rf = read_mem;
		text = textblocks->head;
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
	    if ((rd == 1 && memcmp(out, NL, 1) == 0) ||
		(rd == 2 && memcmp(out, CRLF, 2) == 0) ||
		is_blank_line(out, rd))		/* check for non-empty blank line */
		break;

	    /* rewrite "Subject: " line */
	    if (status == RC_SPAM &&
		rd >= subjlen && 
		spam_subject_tag != NULL &&
		memcmp(out, "Subject:", subjlen) == 0) {
		(void) fprintf(fp, "Subject: %s", spam_subject_tag);
		if (out[subjlen] != ' ')
		    fputc(' ', fp);
		(void) fwrite(out + subjlen, 1, rd - subjlen, fp);
		continue;
	    }

	    hadlf = (out[rd-1] == '\n');
	    (void) fwrite(out, 1, rd, fp);
	    if (ferror(fp)) cleanup_exit(2, 1);
	}

	if (!hadlf)
	    fputc('\n', fp);
    }

    if (passthrough || verbose) {
	typedef char *formatter(char *buff, size_t size);
	formatter *fcn = terse ? format_terse : format_header;
	char buff[256];
	/* print spam-status at the end of the header
	 * then mark the beginning of the message body */
	fcn(buff, sizeof(buff));
	fputs (buff, fp);
	fputs ("\n", fp);
    }

    if (verbose || passthrough || Rtable) {
	verbose += passthrough;
	print_stats( stdout );
	verbose -= passthrough;
    }

    if (passthrough) {
	int hadlf = 1;
	/* If the message terminated early (without body or blank
	 * line between header and body), enforce a blank line to
	 * prevent anything past us from choking. */
	(void)fputc('\n', fp);

	/* print body */
	while ((rd = rf(&out, rfarg)) > 0)
	{
	    (void) fwrite(out, 1, rd, fp);
	    hadlf = (out[rd-1] == '\n');
	    if (ferror(fp)) cleanup_exit(2, 1);
	}

	if (!hadlf) fputc('\n', fp);

	if (fflush(fp) || ferror(fp) || (fp != stdout && fclose(fp))) {
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
	    exit(2);
	}
    } else {
	fp = stdout;
    }
    return fp;
}

static void passthrough_setup()
{
    /* check if the input is seekable, if it is, we don't need to buffer
     * things in memory => configure passmode accordingly
     */

    if (!passthrough)
	return;

    passmode = PASS_MEM;

    if (!mbox_mode) {
	if (fseek(fpin, 0, SEEK_END) == 0) {
	    passmode = PASS_SEEK;
	    (void)rewind(fpin);
	} else {
	    if (errno != ESPIPE && errno != ENOTTY) {
		fprintf(stderr, "cannot determine if input is seekable: %s\n",
			strerror(errno));
		exit(2);
	    }
	}
    }

    if (passmode == PASS_MEM)
	textblocks = textblock_init();

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

static void passthrough_cleanup()
{
    if (!passthrough)
	return;

    switch(passmode) {
    case PASS_MEM:
	textblock_free(textblocks);
	break;
    case PASS_SEEK: default:
	break;
    }
}

/* End */
