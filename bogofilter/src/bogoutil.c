/* $Id$ */

/*****************************************************************************

NAME:
  bogoutil.c -- dumps & loads bogofilter text files from/to Berkeley DB format.

AUTHORS:
  Gyepi Sam    <gyepi@praxis-sw.com>
  David Relson <relson@osagesoftware.com>

******************************************************************************/

#include "common.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
 
#include "getopt.h"

#include "bogofilter.h"
#include "bogohist.h"
#include "bogohome.h"
#include "buff.h"
#include "configfile.h"
#include "datastore.h"
#include "datastore_db.h"
#include "error.h"
#include "maint.h"
#include "msgcounts.h"
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

/* Function Prototypes */

static int process_arg(int option, const char *name, const char *arg);

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

static int dump_wordlist(const char *ds_file)
{
    int rc;

    token_count = 0;

    rc = ds_oper(ds_file, DS_READ, ds_dump_hook, NULL);

    if (rc)
	fprintf(stderr, "error dumping tokens!\n");
    else
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

static int load_wordlist(const char *ds_file)
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

    dsh = ds_open(CURDIR_S, ds_file, DS_WRITE | DS_LOAD);
    if (dsh == NULL)
	return EX_ERROR;

    memset(buf, '\0', BUFSIZE);

    ds_txn_begin(dsh);

    for (;;) {
	dsv_t data;
	word_t *token;
	if (fgets((char *)buf, BUFSIZE, fpin) == NULL) {
	    if (ferror(fpin)) {
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
	    switch (ds_read(dsh, token, &data)) {
		case 0:
		case 1:
		    break;
		default:
		    rv = 1;
	    }
	    data.spamcount += spamcount;
	    data.goodcount += goodcount;
	    if (ds_write(dsh, token, &data)) rv = 1;
	}
	word_free(token);
    }

    if (rv) {
	fprintf(stderr, "read or write error, aborting.\n");
	ds_txn_abort(dsh);
    } else {
	switch (ds_txn_commit(dsh)) {
	    case DST_FAILURE:
	    case DST_TEMPFAIL:
		fprintf(stderr, "commit failed\n");
		exit(EX_ERROR);
	    case DST_OK:
		break;
	}
    }

    ds_close(dsh);

    ds_cleanup();

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
    int rv = 0;

    dsv_t msgcnts;

    /* protect against broken stat(2) that succeeds for empty names */
    if (path == NULL || *path == '\0') {
        fprintf(stderr, "Expecting non-empty directory or file name.\n");
        return EX_ERROR;
    }

    if ( stat(path, &sb) == 0 ) {
	/* XXX FIXME: deadlock possible */
	if ( ! S_ISDIR(sb.st_mode)) {		/* words from file */
	    dsh = ds_open(CURDIR_S, path, DS_READ);
	}
	else {					/* words from path */
	    char filepath[PATH_LEN];

	    build_wordlist_path(filepath, sizeof(filepath), path);

	    dsh = ds_open(CURDIR_S, filepath, DS_READ);
	}
    }

    if (dsh == NULL) {
	fprintf(stderr, "Error accessing file or directory '%s'.\n", path);
	if (errno != 0)
	    fprintf(stderr, "error #%d - %s.\n", errno, strerror(errno));
	return EX_ERROR;
    }

    if (DST_OK != ds_txn_begin(dsh)) {
	ds_close(dsh);
	fprintf(stderr, "Cannot begin transaction.\n");
	return EX_ERROR;
    }

    if (show_probability)
    {
	ds_get_msgcounts(dsh, &msgcnts);
	robs = ROBS;
	robx = ROBX;
    }

    printf(head_format, "", "spam", "good", "  Fisher");
    while (argc >= 0)
    {
	dsv_t val;
	word_t *token;
	int rc;

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

	rc = ds_read(dsh, token, &val);
	switch (rc) {
	    case 0:
		spam_count = val.spamcount;
		good_count = val.goodcount;

		if (!show_probability)
		    printf(data_format, token->text, spam_count, good_count);
		else
		{
		    rob_prob = calc_prob_pure(good_count, spam_count, msgcnts.goodcount, msgcnts.spamcount,
			    robs, robx);
		    printf(data_format, token->text, spam_count, good_count, rob_prob);
		}
		break;
	    case 1:
		break;
	    default:
		fprintf(stderr, "Cannot read from data base.\n");
		rv = EX_ERROR;
		goto finish;
	}

	if (token != &buff->t)
	    word_free(token);
    }

finish:
    if (DST_OK != rv ? ds_txn_abort(dsh) : ds_txn_commit(dsh)) {
	fprintf(stderr, "Cannot %s transaction.\n", rv ? "abort" : "commit");
	rv = EX_ERROR;
    }
    ds_close(dsh);
    ds_cleanup();

    buff_free(buff);

    return rv;
}

static int get_robx(const char *path)
{
    double rx;
    int ret = 0;

    init_wordlist("word", WORDLIST, 0, WL_REGULAR);
    rx = compute_robinson_x(path);
    if (rx < 0)
	return EX_ERROR;

    if (onlyprint)
	printf("%f\n", rx);
    else {
	dsv_t val;
	word_t *word_robx = word_new((const byte *)ROBX_W, (uint) strlen(ROBX_W));

	open_wordlists(word_lists, DS_WRITE);

	if (DST_OK == ds_txn_begin(word_lists->dsh)) {
	    val.goodcount = 0;
	    val.spamcount = (uint32_t) (rx * 1000000);
	    ret = ds_write(word_lists->dsh, word_robx, &val);
	    if (DST_OK != ds_txn_commit(word_lists->dsh))
		ret = 1;
	}

	close_wordlists(word_lists);
	free_wordlists(word_lists);
	word_lists = NULL;

	word_free(word_robx);
    }

    return ret ? EX_ERROR : EX_OK;
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
	    "\t-f dir\tRun recovery on data base in dir.\n"
	    "\t-F dir\tRun catastrophic recovery on data base in dir.\n"
	    "\t-d file\tDump data from file to stdout.\n"
	    "\t-l file\tLoad data from stdin into file.\n"
	    "\t-u file\tUpgrade wordlist version.\n"
	    "\t-w dir\tDisplay counts for words from stdin.\n"
	    "\t-p dir\tDisplay word counts and probabilities.\n"
	    "\t-m\tEnable maintenance works (expiring tokens).\n");
    fprintf(stderr,
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
	    "\t-y date\tSet default date (format YYYYMMDD).\n"
	    "\t-x list\tSet debug flags.\n"
	    "\t-D\tDirect debug output to stdout.\n"
	    "\t-V\tPrint program version.\n"
	    "\t-I file\tRead this file instead of standard input.\n"
	    "\n"
	    "%s (version %s) is part of the bogofilter package.\n",
	    progname, version);
}

static const char *ds_file = NULL;
static bool  prob = false;

typedef enum { M_NONE, M_DUMP, M_LOAD, M_WORD, M_MAINTAIN, M_ROBX, M_HIST,
    M_RECOVER, M_CRECOVER } cmd_t;
static cmd_t flag = M_NONE;

#define	OPTIONS	":a:c:d:Df:F:hH:I:k:l:m:np:r:R:s:u:vVw:x:X:y:"

struct option long_options[] = {
#ifdef	HAVE_DECL_DB_CREATE
    { "db_lk_max_locks",		R, 0, O_DB_MAX_LOCKS },
    { "db_lk_max_objects",		R, 0, O_DB_MAX_OBJECTS },
#ifdef	FUTURE_DB_OPTIONS
    { "db_log_autoremove",		R, 0, O_DB_LOG_AUTOREMOVE },
    { "db_txn_durable",			R, 0, O_DB_TXN_DURABLE },
#endif
#endif
#ifdef	VERSION_0932
    { "debug-flags",			R, 0, 'x' },
    { "debug-to-stdout",		N, 0, 'D' },
    { "verbosity",			N, 0, 'v' },
    /* The following options are present to allow config files
    ** to be read without complaints (and mostly ignored)
    */
    { "config-file",			N, 0, O_IGNORE },
    { "no-config-file",			N, 0, O_IGNORE },
    { "no-header-tags",			N, 0, O_IGNORE },
    { "input-file",			N, 0, O_IGNORE },
    { "output-file",			N, 0, O_IGNORE },
    { "query",				N, 0, O_IGNORE },
    { "block_on_subnets",		R, 0, O_IGNORE },
    { "bogofilter_dir",			R, 0, O_IGNORE },
    { "charset_default",		R, 0, O_IGNORE },
    { "ham_cutoff",			R, 0, O_IGNORE },
    { "header_format",			R, 0, O_IGNORE },
    { "log_header_format",		R, 0, O_IGNORE },
    { "log_update_format",		R, 0, O_IGNORE },
    { "min_dev",			R, 0, O_IGNORE },
    { "replace_nonascii_characters",	R, 0, O_IGNORE },
    { "robs",				R, 0, O_IGNORE },
    { "robx",				R, 0, O_IGNORE },
    { "spam_cutoff",			R, 0, O_IGNORE },
    { "spam_header_name",		R, 0, O_IGNORE },
    { "spam_subject_tag",		R, 0, O_IGNORE },
    { "spamicity_formats",		R, 0, O_IGNORE },
    { "spamicity_tags",			R, 0, O_IGNORE },
    { "stats_in_header",		R, 0, O_IGNORE },
    { "terse",				R, 0, O_IGNORE },
    { "terse_format",			R, 0, O_IGNORE },
    { "thresh_update",			R, 0, O_IGNORE },
    { "timestamp",			R, 0, O_IGNORE },
    { "unsure_subject_tag",		R, 0, O_IGNORE },
    { "user_config_file",		R, 0, O_IGNORE },
#endif
    { NULL,				0, 0, 0 }
};

static int process_arglist(int argc, char **argv)
{
    int option;
    int count = 0;

    fpin = stdin;
    dbgout = stderr;

#ifdef __EMX__
    _response (&argc, &argv);	/* expand response files (@filename) */
    _wildcard (&argc, &argv);	/* expand wildcards (*.*) */
#endif

    while (1)
    {
	int option_index = 0;
	int this_option_optind = optind ? optind : 1;
	const char *name;

	option = getopt_long(argc, argv, OPTIONS,
			     long_options, &option_index);

	if (option == -1)
 	    break;

	name = (option_index == 0) ? argv[this_option_optind] : long_options[option_index].name;
	count += process_arg(option, name, optarg);
    }

    if (count != 1)
    {
	fprintf(stderr, "%s: Exactly one of the -d, -f, -F, -l, -R or -w flags "
		"must be present.\n", progname);
	exit(EX_ERROR);
    }
 
    if (optind < argc && flag != M_WORD) {
	fprintf(stderr, "Extra arguments given, first: %s. Aborting.\n", argv[optind]);
	exit(EX_ERROR);
    }

    return count;
}

static int process_arg(int option, const char *name, const char *val)
{
    int count = 0;

    switch (option) {
    case '?':
	fprintf(stderr, "Unknown option '%s'.\n", name);
	break;

    case 'f':
	flag = M_RECOVER;
	count += 1;
	ds_file = val;
	break;

    case 'F':
	flag = M_CRECOVER;
	count += 1;
	ds_file = val;
	break;

    case 'd':
	flag = M_DUMP;
	count += 1;
	ds_file = val;
	break;

    case 'k':
	db_cachesize=(uint) atoi(val);
	break;

    case 'l':
	flag = M_LOAD;
	count += 1;
	ds_file = val;
	break;

    case 'm':
	flag = M_MAINTAIN;
	count += 1;
	ds_file = val;
	break;

    case 'p':
	prob = true;
	/*@fallthrough@*/

    case 'w':
	flag = M_WORD;
	count += 1;
	ds_file = val;
	break;

    case 'r':
	onlyprint = true;
    case 'R':
	flag = M_ROBX;
	count += 1;
	ds_file = val;
	break;

    case 'u':
	upgrade_wordlist_version = true;
	flag = M_MAINTAIN;
	count += 1;
	ds_file = val;
	break;

    case 'v':
	verbose++;
	break;

    case ':':
	fprintf(stderr, "Option -%c requires an argument.\n", optopt);
	exit(EX_ERROR);

    case 'h':
	help();
	exit(EX_OK);

    case 'H':
	flag = M_HIST;
	count += 1;
	ds_file = val;
	break;

    case 'V':
	print_version();
	exit(EX_OK);

    case 'x':
	set_debug_mask(val);
	break;

    case 'X':
	set_bogotest(val);
	break;

    case 'a':
	maintain = true;
	thresh_date = string_to_date(val);
	break;

    case 'c':
	maintain = true;
	thresh_count = (uint) atoi(val);
	break;

    case 's':
    {
	unsigned long mi, ma;

	maintain = true;
	    
	if (2 == sscanf(val, "%lu,%lu", &mi, &ma)) {
	    size_min = mi;
	    size_max = ma;
	} else {
	    fprintf(stderr, "syntax error in argument \"%s\" of -s\n.",
		    val);
	    exit(EX_ERROR);
	}
    }
    break;

    case 'n':
	maintain = true;
	replace_nonascii_characters ^= true;
	break;

    case 'y':		/* date as YYYYMMDD */
    {
	YYYYMMDD date = string_to_date(val);
	maintain = true;
	if (date != 0 && date < 19990000) {
	    fprintf(stderr, "Date format for '-y' option is YYYYMMDD\n");
	    exit(EX_ERROR);
	}
	set_date( date );
	break;
    }

    case 'I':
	fpin = fopen( val, "r" );
	if (fpin == NULL) {
	    fprintf(stderr, "Can't read file '%s'\n", val);
	    exit(EX_ERROR);
	}
	break;

    case 'D':
	dbgout = stdout;
	break;

#ifdef	HAVE_DECL_DB_CREATE
    case O_DB_MAX_OBJECTS:	db_max_objects = atoi(val);		  break;
    case O_DB_MAX_LOCKS:	db_max_locks   = atoi(val);		  break;
#ifdef	FUTURE_DB_OPTIONS
    case O_DB_LOG_AUTOREMOVE:	db_log_autoremove = get_bool(name, val);  break;
    case O_DB_TXN_DURABLE:	db_txn_durable    = get_bool(name, val);  break;
#endif
#endif

    default:
	abort();
    }

    return count;
}

int main(int argc, char *argv[])
{
    int rc;
    progtype = build_progtype(progname, DB_TYPE);

    set_today();			/* compute current date for token age */

    process_arglist(argc, argv);

    /* Extra or missing parameters */
    if (flag != M_WORD && argc != optind) {
	usage();
	exit(EX_ERROR);
    }

    atexit(bf_exit);

    set_bogohome(ds_file);

    if (flag == M_RECOVER) {
	return ds_recover(0);
    } else if (flag == M_CRECOVER) {
	return ds_recover(1);
    }

    ds_init();

    switch(flag) {
	case M_DUMP:
	    rc = dump_wordlist(ds_file);
	    break;
	case M_LOAD:
	    rc = load_wordlist(ds_file);
	    break;
	case M_MAINTAIN:
	    maintain = true;
	    rc = maintain_wordlist_file(ds_file);
	    break;
	case M_WORD:
	    argc -= optind;
	    argv += optind;
	    rc = display_words(ds_file, argc, argv, prob);
	    break;
	case M_HIST:
	    rc = histogram(ds_file);
	    break;
	case M_ROBX:
	    rc = get_robx(ds_file);
	    break;
	case M_NONE:
	default:
	    /* should have been handled above */
	    abort();
	    break;
    }

    ds_cleanup();

    return rc;
}
