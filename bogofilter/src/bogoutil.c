/* $Id$ */

/*****************************************************************************

NAME:
  bogoutil.c -- dumps & loads bogofilter text files from/to Berkeley DB format.

AUTHOR:
  Gyepi Sam <gyepi@praxis-sw.com>

******************************************************************************/

#include "common.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#undef	HAVE_DECL_GETOPT	/* for configure/getopt incompatibility */
#include "getopt.h"

#include "bogofilter.h"
#include "bogohist.h"
#include "buff.h"
#include "datastore.h"
#include "error.h"
#include "maint.h"
#include "paths.h"
#include "prob.h"
#include "robx.h"
#include "swap.h"
#include "wordlists.h"
#include "xmalloc.h"
#include "xstrdup.h"

const char *progname = "bogoutil";

static int token_count = 0;

bool  maintain = false;
bool  onlyprint = false;

/* Function Definitions */

static int ds_dump_hook(word_t *key, dsv_t *data,
			/*@unused@*/ void *userdata)
/* returns 0 if ok, 1 if not ok */
{
    (void)userdata;

    token_count += 1;

    if (maintain && discard_token(key, data))
	return 0;

    if (replace_nonascii_characters)
	do_replace_nonascii_characters(key->text, key->leng);

    printf( "%.*s %lu %lu",
	    CLAMP_INT_MAX(key->leng), key->text,
	    (unsigned long)data->spamcount,
	    (unsigned long)data->goodcount);
    if (data->date)
	printf(" %lu", (unsigned long)data->date);
    putchar('\n');

    fflush(stdout); /* solicit ferror flag if output is shorter than buffer */
    return ferror(stdout) ? 1 : 0;
}

static int dump_file(char *ds_file)
{
    int rc;

    token_count = 0;

    rc = ds_oper(ds_file, DB_READ, ds_dump_hook, NULL);

    if (verbose)
	fprintf(dbgout, "%d tokens dumped\n", token_count);

    return rc;
}

#define BUFSIZE 512

static byte *spanword(byte *t)
{
    while (isspace(*t)) t += 1;		/* skip leading whitespace */
    while (*t && !isspace(*t)) t += 1;	/* span current word	   */
    if (*t)
	*t++ = '\0';
    return t;
}

static int load_file(const char *ds_file)
{
    void *dsh;
    byte buf[BUFSIZE];
    byte *p;
    int rv = 0;
    size_t len;
    int load_count = 0;
    unsigned long line = 0;
    unsigned long count[IX_SIZE], date;
    YYYYMMDD today_save = today;

    dsh = ds_open(CURDIR_S, ds_file, DB_WRITE);
    if (dsh == NULL)
	return EX_ERROR;

    memset(buf, '\0', BUFSIZE);

    for (;;) {
	dsv_t data;
	word_t *token;
	if (fgets((char *)buf, BUFSIZE, stdin) == NULL) {
	    if (ferror(stdin)) {
		perror(progname);
		rv = 2;
	    }
	    break;
	}

	line++;

	len = strlen((char *)buf);

	/* too short. */
	if (len < 4)
	    continue;

	p = spanword(buf);
	len = strlen((const char *)buf);

	if (len > MAXTOKENLEN)
	    continue;		/* too long - discard */

	spamcount = (uint) atoi((const char *)p);
	if ((int) spamcount < 0)
	    spamcount = 0;
	p = spanword(p);

	goodcount = (uint) atoi((const char *)p);
	if ((int) goodcount < 0)
	    goodcount = 0;
	p = spanword(p);

	date = (uint) atoi((const char *)p);
	p = spanword(p);

	if (*p != '\0') {
	    fprintf(stderr,
		    "%s: Unexpected input [%s] on line %lu. "
		    "Expecting whitespace before count.\n",
		    progname, buf, line);
	    rv = 1;
	    break;
	}

	if (date == 0)				/* date as YYYYMMDD */
	    date = today_save;

	if (replace_nonascii_characters)
	    do_replace_nonascii_characters(buf, len);
 
 	token = word_new(buf, len);
	data.goodcount = goodcount;
	data.spamcount = spamcount;
	data.date = date;

	if (! (maintain && discard_token(token, &data))) {
	    load_count += 1;
	    /* Slower, but allows multiple lists to be concatenated */
	    set_date(date);
	    ds_read(dsh, token, &data);
	    data.spamcount += spamcount;
	    data.goodcount += goodcount;
	    ds_write(dsh, token, &data);
	}
	word_free(token);
    }

    ds_close(dsh, false);

    if (verbose)
	fprintf(dbgout, "%d tokens loaded\n", load_count);

    return rv;
}

static int get_token(buff_t *buff, FILE *fp)
{
    int rv = 0;

    if (fgets((char *)buff->t.text, buff->size, fp) == NULL) {
	if (ferror(fp)) {
	    perror(progname);
	    rv = 2;
	} else {
	    rv = 1;
	}
    } else {
	buff->t.leng = (uint) strlen((const char *)buff->t.text);
	if (buff->t.text[buff->t.leng - 1] == '\n' ) {
	    buff->t.leng -= 1;
	    buff->t.text[buff->t.leng] = (byte) '\0';
	}
	else
	{
	    fprintf(stderr,
		    "%s: Unexpected input [%s]. Does not end with newline "
		    "or line too long.\n",
		    progname, buff->t.text);
	    rv = 1;
	}
    }
    return rv;
}

static int display_words(const char *path, int argc, char **argv, bool show_probability)
{
    byte buf[BUFSIZE];
    buff_t *buff = buff_new(buf, 0, BUFSIZE);
    const byte *word = buf;

    const char *head_format = !show_probability ? "%-30s %6s %6s\n"   : "%-30s %6s  %6s  %6s\n";
    const char *data_format = !show_probability ? "%-30s %6lu %6lu\n" : "%-30s %6lu  %6lu  %f\n";

    void *dsh = NULL; /* initialize to silence bogus gcc warning */

    struct stat sb;

    /* protect against broken stat(2) that succeeds for empty names */
    if (path == NULL || *path == '\0') {
        fprintf(stderr, "Expecting non-empty directory or file name.\n");
        return EX_ERROR;
    }

    if ( stat(path, &sb) == 0 ) {
	/* XXX FIXME: deadlock possible */
	if ( ! S_ISDIR(sb.st_mode)) {		/* words from file */
	    dsh = ds_open(CURDIR_S, path, DB_READ);
	}
	else {					/* words from path */
	    char filepath[PATH_LEN];

	    build_wordlist_path(filepath, sizeof(filepath), path);

	    dsh = ds_open(CURDIR_S, filepath, DB_READ);
	}
    }

    if (dsh == NULL) {
	fprintf(stderr, "Error accessing file or directory '%s'.\n", path);
	if (errno != 0)
	    fprintf(stderr, "error #%d - %s.\n", errno, strerror(errno));
	return EX_ERROR;
    }

    if (show_probability)
    {
	dsv_t val;
	ds_get_msgcounts(dsh, &val);
	msgs_good = val.goodcount;
	msgs_bad  = val.spamcount;
	robs = ROBS;
	robx = ROBX;
    }

    printf(head_format, "", "spam", "good", "  Fisher");

    while (argc >= 0)
    {
	dsv_t val;
	word_t *token;

	unsigned long spam_count;
	unsigned long good_count;
	double rob_prob = 0.0;
	
	if (argc == 0)
	{
	    if (get_token(buff, stdin) != 0)
		break;
	    token = &buff->t;
	} else {
	    word = (const byte *) *argv++;
	    if (--argc == 0)
		argc = -1;
	    token = word_new(word, (uint) strlen((const char *)word));
	}

	ds_read(dsh, token, &val);
	spam_count = val.spamcount;
	good_count = val.goodcount;

	if (!show_probability)
	    printf(data_format, token->text, spam_count, good_count);
	else
	{
	    rob_prob = calc_prob(good_count, spam_count);
	    printf(data_format, token->text, spam_count, good_count, rob_prob);
	}

	if (token != &buff->t)
	    word_free(token);
    }

    ds_close(dsh, false);

    buff_free(buff);

    return 0;
}

static int get_robx(char *path)
{
    double rx;

    rx = compute_robinson_x(path);

    if (onlyprint)
	printf("%f\n", rx);
    else {
	dsv_t val;
	void  *dsh;
	char filepath[PATH_LEN];

	word_t *word_robx = word_new((const byte *)ROBX_W, (uint) strlen(ROBX_W));
	    
	build_wordlist_path(filepath, sizeof(filepath), path);
	
	run_type = REG_SPAM;

	dsh = ds_open(CURDIR_S, filepath, DB_WRITE);
	if (dsh == NULL)
	    return EX_ERROR;

	val.goodcount = 0;
	val.spamcount = (uint32_t) (rx * 1000000);
	ds_write(dsh, word_robx, &val);
	ds_close(dsh, false);

	word_free(word_robx);
    }

    return EX_OK;
}

static void print_version(void)
{
    (void)fprintf(stderr,
		  "%s version %s\n"
		  "    Database: %s\n"
		  "Copyright (C) 2002-2004 Gyepi Sam, David Relson\n\n"
		  "%s comes with ABSOLUTELY NO WARRANTY.  "
		  "This is free software, and\nyou are welcome to "
		  "redistribute it under the General Public License.  "
		  "See\nthe COPYING file with the source distribution for "
		  "details.\n"
		  "\n", 
		  progtype, version, ds_version_str(), PACKAGE);
}

static void usage(void)
{
    fprintf(stderr, "Usage: %s { -d | -l | -w | -p } file%s | { -r | -R } directory | [ -v | -h | -V ]\n", 
	    progname, DB_EXT);
}

static void help(void)
{
    usage();
    fprintf(stderr,
	    "\n"
	    "\t-h\tPrint this message.\n"
	    "\t-d file\tDump data from file to stdout.\n"
	    "\t-l file\tLoad data from stdin into file.\n"
	    "\t-w dir\tDisplay counts for words from stdin.\n"
	    "\t-p dir\tDisplay word counts and probabilities.\n"
	    "\t-m\tEnable maintenance works (expiring tokens).\n"
	    "\t-v\tOutput debug messages.\n"
	    "\t-H dir\tDisplay histogram and statistics for the wordlist.\n"
	    "\t"    "\tUse -v  to exclude hapaxes."
	    "\t"    "\tUse -vv to exclude pure spam/ham.\n"
	    "\t-r dir\tCompute Robinson's X for specified directory.\n"
	    "\t-R dir\tCompute Robinson's X and save it in the wordlist.\n");
    fprintf(stderr,
	    "\t-k size\tset BerkeleyDB cache size (MB).\n"
	    DB_EXT);
    fprintf(stderr,
	    "\t-a age\tExclude tokens with older ages.\n"
	    "\t-c cnt\tExclude tokens with lower counts.\n"
	    "\t-s l,h\tExclude tokens with lengths between 'l' and 'h' (low and high).\n"
	    "\t-n\tReplace non-ascii characters with '?'.\n"
	    "\t-y day\tSet default day-of-year (1..366).\n"
	    "\t-x list\tSet debug flags.\n"
	    "\t-D\tDirect debug output to stdout.\n"
	    "\t-V\tPrint program version.\n"
	    "\t-I file\tRead this file instead of standard input.\n"
	    "\n"
	    "%s (version %s) is part of the bogofilter package.\n",
	    progname, version);
}

static char *ds_file = NULL;
static bool  prob = false;

typedef enum { M_NONE, M_DUMP, M_LOAD, M_WORD, M_MAINTAIN, M_ROBX, M_HIST } cmd_t;
static cmd_t flag = M_NONE;

#define	OPTIONS	":a:c:d:DhH:I:k:l:m:np:r:R:s:vVw:x:X:y:"

static int process_arglist(int argc, char **argv)
{
    int option;
    int count = 0;

    fpin = stdin;
    dbgout = stderr;

    while ((option = getopt(argc, argv, OPTIONS)) != -1)
	switch (option) {
	case 'd':
	    flag = M_DUMP;
	    count += 1;
	    ds_file = (char *) optarg;
	    break;

	case 'k':
	    db_cachesize=(uint) atoi(optarg);
	    break;

	case 'l':
	    flag = M_LOAD;
	    count += 1;
	    ds_file = (char *) optarg;
	    break;

	case 'm':
	    flag = M_MAINTAIN;
	    count += 1;
	    ds_file = (char *) optarg;
	    break;

	case 'p':
	    prob = true;
	    /*@fallthrough@*/

	case 'w':
	    flag = M_WORD;
	    count += 1;
	    ds_file = (char *) optarg;
	    break;

	case 'r':
	    onlyprint = true;
	case 'R':
	    flag = M_ROBX;
	    count += 1;
	    ds_file = (char *) optarg;
	    break;

	case 'v':
	    verbose++;
	    break;

	case ':':
	    fprintf(stderr, "Option -%c requires an argument.\n", optopt);
	    exit(EX_ERROR);

	case '?':
	    fprintf(stderr, "Unknown option -%c.\n", optopt);
	    exit(EX_ERROR);

	case 'h':
	    help();
	    exit(EX_OK);

	case 'H':
	    flag = M_HIST;
	    count += 1;
	    ds_file = (char *) optarg;
	    break;

	case 'V':
	    print_version();
	    exit(EX_OK);

	case 'x':
	    set_debug_mask( (char *) optarg );
	    break;

	case 'X':
	    set_bogotest( optarg );
	    break;

	case 'a':
	    maintain = true;
	    thresh_date = string_to_date((char *)optarg);
	    break;

	case 'c':
	    maintain = true;
	    thresh_count = (uint) atoi((char *)optarg);
	    break;

	case 's':
	{
	    unsigned long mi, ma;

	    maintain = true;
	    
	    if (2 == sscanf((const char *)optarg, "%lu,%lu", &mi, &ma)) {
		size_min = mi;
		size_max = ma;
	    } else {
		fprintf(stderr, "syntax error in argument \"%s\" of -s\n.",
			optarg);
		exit(EX_ERROR);
	    }
	}
	break;

	case 'n':
	    maintain = true;
	    replace_nonascii_characters ^= true;
	    break;

	case 'y':		/* date as YYYYMMDD */
	    maintain = true;
	    today = string_to_date((char *)optarg);
	    break;

	case 'I':
	    fpin = fopen( optarg, "r" );
	    if (fpin == NULL) {
		fprintf(stderr, "Can't read file '%s'\n", optarg);
		exit(EX_ERROR);
	    }
	    break;

	case 'D':
	    dbgout = stdout;
	    break;

	default:
	    abort();
	}

    if (count != 1)
    {
      fprintf(stderr, "%s: Exactly one of the -d, -l, -R or -w flags "
	      "must be present.\n", progname);
      exit(EX_ERROR);
    }

    if (optind < argc && flag != M_WORD) {
	fprintf(stderr, "Extra arguments given, first: %s. Aborting.\n", argv[optind]);
	exit(EX_ERROR);
    }

    return count;
}

int main(int argc, char *argv[])
{
    progtype = build_progtype(progname, DB_TYPE);

    set_today();			/* compute current date for token age */

    process_arglist(argc, argv);

    /* Extra or missing parameters */
    if (flag != M_WORD && argc != optind) {
	usage();
	exit(EX_ERROR);
    }

    atexit(bf_exit);

    switch(flag) {
	case M_DUMP:
	    return dump_file(ds_file);
	case M_LOAD:
	    return load_file(ds_file);
	case M_MAINTAIN:
	    maintain = true;
	    return maintain_wordlist_file(ds_file);
	case M_WORD:
	    argc -= optind;
	    argv += optind;
	    return display_words(ds_file, argc, argv, prob);
	case M_HIST:
	    return histogram(ds_file);
	case M_ROBX:
	    return get_robx(ds_file);
	case M_NONE:
	default:
	    /* should have been handled above */
	    abort();
    }
}
