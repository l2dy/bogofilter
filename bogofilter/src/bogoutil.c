/* $Id$ */

/*****************************************************************************

NAME:
   bogoutil.c -- dumps and loads bogofilter text files from/to Berkeley DB format.

AUTHOR:
   Gyepi Sam <gyepi@praxis-sw.com>
   
******************************************************************************/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#include <db.h>

#include <config.h>
#include "common.h"
#include "xmalloc.h"
#include "xstrdup.h"

#include "bogofilter.h"
#include "datastore.h"
#include "error.h"
#include "maint.h"
#include "robinson.h"			/* for ROBS and ROBX */
#include "swap.h"
#include "paths.h"

#define PROGNAME "bogoutil"

#define MINIMUM_FREQ	5		/* minimum freq */

run_t run_type = RUN_NORMAL;

const char *progname = PROGNAME;

static int dump_count = 0;

/* Function Definitions */

static int db_dump_hook(char *key,  uint32_t keylen, 
			char *data, uint32_t datalen,
			 /*@unused@*/ void *userdata)
{
    dbv_t val = {0, 0};
    (void)userdata;

    dump_count += 1;

    if (datalen != sizeof(uint32_t) && datalen != 2 * sizeof(uint32_t)) {
	print_error(__FILE__, __LINE__, "Unknown data size - %d.\n", datalen);
	return 0;
    }

    memcpy(&val, data, datalen);

    if (!keep_count(val.count) || !keep_date(val.date) || !keep_size(keylen))
	return 0;
    if (replace_nonascii_characters)
	do_replace_nonascii_characters((byte *)key, keylen);
    fwrite(key, 1, keylen, stdout);
    putchar(' ');
    printf("%lu", (unsigned long)val.count);
    if (val.date) {
	printf(" %lu", (unsigned long)val.date);
    }
    putchar('\n');
    return !!ferror(stdout);
}

static int count_hook(char *key,  uint32_t keylen, 
		      char *data, uint32_t datalen,
		      void *userdata)
{
    uint32_t *counter = userdata;

    (void)key;
    (void)keylen;
    (void)data;
    (void)datalen;

    (*counter)++;

    if (verbose > 3) {
	printf("count: ");
	fwrite(key, 1, keylen, stdout);
	putchar('\n');
    }

    return 0;
}

struct robhook_data {
    double *sum;
    uint32_t *count;
    void *dbh_good;
    double scalefactor;
};

static int robx_hook(char *key,  uint32_t keylen, 
		     char *data, uint32_t datalen, 
		     void *userdata)
{
    struct robhook_data *rd = userdata;

    uint32_t goodness;
    uint32_t spamness;
    double   prob;
    static word_t *x;
    static size_t x_size = MAXTOKENLEN + 1;

    /* ignore system meta-data */
    if (*key == '.')
	return 0;

    /* ignore short read */
    if (datalen < sizeof(uint32_t))
	return 0;

    if (x == NULL || keylen + 1 > x_size) {
	if (x) word_free(x);
	x_size = max(x_size, keylen + 1);
	x = word_new(NULL, x_size);
    }

    x->leng = keylen;
    memcpy(x->text, key, keylen);
    x->text[keylen] = '\0';

    memcpy(&spamness, data, sizeof(uint32_t));
    goodness = db_getvalue(rd->dbh_good, x);

    prob = spamness / (goodness * rd->scalefactor + spamness);
    (*rd->sum) += prob;

    /* tokens in good list were already counted */
    /* now add in tokens only in spam list */
    if (goodness == 0)
	(*rd->count) ++;

    /* print if token in both word lists */
    if ((verbose > 1 && goodness && spamness) || verbose > 2) {
	printf("cnt: %4lu,  sum: %11.6f,  ratio: %9.6f,  sp: %3lu,  gd: %3lu,"
		"  p: %9.6f,  t: ", (unsigned long)*rd->count, *rd->sum,
		*rd->sum / *rd->count,
		(unsigned long)spamness, (unsigned long)goodness, prob);
	word_puts(x, stdout);
	fputc( '\n', stdout);
    }
    return 0;
}

static int db_oper(const char *path, dbmode_t open_mode, db_foreach_t funct,
	void *userdata) {
    void *dbh;

    if ((dbh = db_open(path, path, open_mode, directory)) == NULL) {
	exit(2);
    } else {
	int r = db_foreach(dbh, funct, userdata);
	if (r) {
	    db_close(dbh, false);
	    return r;
	}
    }
    return 0;
}

static int dump_file(char *db_file)
{
    int rc;

    dump_count = 0;

    rc = db_oper(db_file, DB_READ, db_dump_hook, NULL);

    if (verbose)
	fprintf(dbgout, "%d tokens dumped\n", dump_count);

    return rc;
}

#define BUFSIZE 512

static char *spanword(char *t)
{
    while ( isspace(*t)) t += 1;	/* skip leading whitespace  */
    while (*t && !isspace(*t)) t += 1;	/* span current word        */
    if (*t)
	*t++ = '\0';
    return t;
}

static int load_file(char *db_file)
{
    void *dbh;
    byte buf[BUFSIZE];
    byte *p;
    int rv = 0;
    size_t len;
    int load_count = 0;
    unsigned long line = 0;
    unsigned long count, date;
    YYYYMMDD today_save = today;

    if ((dbh = db_open(db_file, db_file, DB_WRITE, directory)) == NULL)
	return 2;

    memset(buf, '\0', BUFSIZE);

    for (;;) {
	word_t *token;
	if (fgets((char *)buf, BUFSIZE, stdin) == NULL) {
	    if (ferror(stdin)) {
		perror(PROGNAME);
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
	len = strlen(buf);

	count = atoi(p);
	p = spanword(p);

	date = atoi(p);
	p = spanword(p);

	if ( *p != '\0' ) {
	    fprintf(stderr,
		    "%s: Unexpected input [%s] on line %lu. "
		    "Expecting whitespace before count\n",
		    PROGNAME, buf, line);
	    rv = 1;
	    break;
	}

	if (date == 0)				/* date as YYYYMMDD */
	    date = today_save;

	if (replace_nonascii_characters)
	    do_replace_nonascii_characters(buf, len);

	if (!keep_count(count) || !keep_date(date) || !keep_size(strlen((const char *)buf)))
	    continue;

	load_count += 1;

	/* Slower, but allows multiple lists to be concatenated */
	set_date(date);
	token = word_new(buf, len);
	count += db_getvalue(dbh, token);
	db_setvalue(dbh, token, count);
	word_free(token);
    }
    db_close(dbh, false);

    if (verbose)
	fprintf(dbgout, "%d tokens loaded\n", load_count);

    return rv;
}

static int get_token(char *buf, int bufsize, FILE *fp)
{
    char *p;
    int rv = 0;
    size_t len;

    if (fgets(buf, bufsize, fp) == NULL) {
	if (ferror(fp)) {
	    perror(PROGNAME);
	    rv = 2;
	} else {
	    rv = 1;
	}
    } else {
	len = strlen(buf);
	p = &buf[len - 1];
	
	if (*(p--) != '\n') {
	    fprintf(stderr,
		    "%s: Unexpected input [%s]. Does not end with newline "
		    "or line too long.\n",
		    PROGNAME, buf);
	    rv = 1;
	}
	*(p + 1) = '\0';
    }
    return rv;
}

static int words_from_list(const char *db_file, int argc, char **argv)
{
    void *dbh;
    int rv = 0;

    dbh = db_open(db_file, db_file, DB_READ, directory);
    if ( dbh == NULL )
	return 2;

    if ( argc == 0)
    {
	char buf[BUFSIZE];
	while (get_token(buf, BUFSIZE, stdin) == 0) {
	    word_t *token = word_new(buf, strlen(buf));
	    uint32_t count = db_getvalue(dbh, token);
	    word_puts(token, stdout);
	    printf(" %lu\n", (unsigned long) count);
	    word_free(token);
	}
    }
    else
    {
	while (argc-- > 0) {
	    char *word = *argv++;
	    word_t *token = word_new(word, strlen(word));
	    uint32_t count = db_getvalue(dbh, token);
	    word_puts(token, stdout);
	    printf(" %lu\n", (unsigned long) count);
	    word_free(token);
	}
    }

    db_close(dbh, false);

    return rv;
}

static int words_from_path(const char *dir, int argc, char **argv, bool show_probability)
{
    void *dbh_good;
    void *dbh_spam;
    char filepath[PATH_LEN];
    char buf[BUFSIZE];
    char *word = buf;
    unsigned long spam_count, spam_msg_count = 0 ;
    unsigned long good_count, good_msg_count = 0 ;

    const char *head_format = !show_probability ? "%-20s %6s %6s\n"   : "%-20s %6s  %6s  %6s  %6s\n";
    const char *data_format = !show_probability ? "%-20s %6ld %6ld\n" : "%-20s %6ld  %6ld  %f  %f\n";

    /* XXX FIXME: deadlock possible */
    if (build_path(filepath, sizeof(filepath), dir, GOODFILE) < 0)
	return 2;

    if ((dbh_good = db_open(filepath, GOODFILE, DB_READ, directory)) == NULL)
	return 2;

    if (build_path(filepath, sizeof(filepath), dir, SPAMFILE) < 0)
	return 2;

    if ((dbh_spam = db_open(filepath, SPAMFILE, DB_READ, directory)) == NULL)
	return 2;

    if (show_probability)
    {
	spam_msg_count = db_get_msgcount(dbh_spam);
	good_msg_count = db_get_msgcount(dbh_good);
    }

    printf(head_format, "", "spam", "good", "Gra prob", "Rob prob");

    while (argc >= 0)
    {
	word_t *token;
	double gra_prob = 0.0f, rob_prob = 0.0f;
	
	if ( argc == 0)
	{
	    if (get_token(buf, BUFSIZE, stdin) != 0)
		break;
	    token = word_new(buf, strlen(buf));
	} else {
	    word = *argv++;
	    if (--argc == 0)
		argc = -1;
	    token = word_new(word, strlen(word));
	}

	spam_count = db_getvalue(dbh_spam, token);
	good_count = db_getvalue(dbh_good, token);

	if (show_probability)
	{
	    double spamness = (double) spam_count / (double) spam_msg_count;
	    double goodness = (double) good_count / (double) good_msg_count;

	    gra_prob = (spam_count + good_count <= MINIMUM_FREQ)
		? UNKNOWN_WORD
		: spamness / (spamness+goodness);
	    rob_prob = ((ROBS * ROBX + spamness) / (ROBS + spamness+goodness));
	}
	printf(data_format, token->text, spam_count, good_count, gra_prob, rob_prob);
	word_free(token);
    }

    db_close(dbh_good, false);
    db_close(dbh_spam, false);

    return 0;
}

static int display_words(const char *path, int argc, char **argv, bool prob)
{
    struct stat sb;
    int rc;

    /* protect against broken stat(2) that succeeds for empty names */
    if (*path)
	rc = stat(path, &sb);
    else
	rc = -1, errno = ENOENT;

    if (rc >= 0) 
    {
	if ( S_ISDIR(sb.st_mode))
	    words_from_path(path, argc, argv, prob);
	else
    	    words_from_list(path, argc, argv);
    } else {
	if (errno==ENOENT) {
	    fprintf(stderr, "No such directory.\n");
	    return 0;
	}
	else {
	    perror("Error accessing directory");
	    return -1;
	}
    }
    return 0;
}

static double compute_robx(void *dbh_spam, void *dbh_good)
{
    size_t good_cnt = 0;
    size_t spam_cnt = 0;
    double sum = 0.0;
    double robx;

    uint32_t msg_good, msg_spam;
    struct robhook_data rh;

    msg_good = db_get_msgcount( dbh_good );
    msg_spam = db_get_msgcount( dbh_spam );
    rh.scalefactor = (double)msg_spam/msg_good;
    rh.dbh_good = dbh_good;
    rh.sum = &sum;
    rh.count = &spam_cnt;

    db_foreach(dbh_good, count_hook, &good_cnt);
    db_foreach(dbh_spam, robx_hook, &rh);

    robx = sum/(spam_cnt + good_cnt);
    if (verbose)
	printf( ".MSG_COUNT: %lu, %lu, scale: %f, sum: %f, cnt: %6d, .ROBX: %f\n",
		(unsigned long)msg_spam, (unsigned long)msg_good, rh.scalefactor, sum, (int)(spam_cnt + good_cnt), robx);

    return robx;
}

static int compute_robinson_x(char *path)
{
    wordlist_t wl[2];

    double robx;
    word_t *word_robx = word_new(ROBX_W, strlen(ROBX_W));

    void *dbh_spam;

    char db_spam_file[PATH_LEN];
    char db_good_file[PATH_LEN];

    if (build_path(db_spam_file, sizeof(db_spam_file), path, SPAMFILE) < 0 ||
	build_path(db_good_file, sizeof(db_good_file), path, GOODFILE) < 0 )
    {
	fprintf(stderr, "%s: string too long creating .db file name.\n", PROGNAME);
	exit(2);
    }

    memset(wl, 0, sizeof(wl));

    wl[0].next = &wl[1];
    wl[0].filepath = db_good_file;
    wl[0].filename = xstrdup("good");

    wl[1].next = NULL;
    wl[1].filepath = db_spam_file;
    wl[1].filename = xstrdup("spam");

    word_lists = wl;
    open_wordlists(DB_READ);

    robx = compute_robx(wl[1].dbh, wl[0].dbh);
    close_wordlists(false);
    free(wl[0].filename);
    free(wl[1].filename);

    dbh_spam = db_open(db_spam_file, "spam", DB_WRITE, directory);
    db_setvalue(dbh_spam, word_robx, (uint32_t) (robx * 1000000));
    db_close(dbh_spam, false);

    word_free(word_robx);

    return 0;
}

static void print_version(void)
{
    fprintf(stderr,
	    "%s: version: %s\n"
	    "Copyright (C) 2002 Gyepi Sam\n\n"
	    "%s comes with ABSOLUTELY NO WARRANTY.\n"
	    "This is free software, and you are welcome to redistribute\n"
	    "it under the General Public License.\n"
	    "See the COPYING file with the source distribution for details.\n\n",
	    PROGNAME, version, PROGNAME);
}

static void usage(void)
{
    fprintf(stderr, "Usage: %s { -d | -l | -w } file.db | -R directory | [ -p | -v | -h | -V ]\n", PROGNAME);
}

static void help(void)
{
    usage();
    fprintf(stderr,
	    "\t-d file\tDump data from file to stdout.\n"
	    "\t-l file\tLoad data from stdin into file.\n"
	    "\t-w\tDisplay counts for words from stdin.\n"
	    "\t-p\tOutput word probabilities.\n"
	    "\t-v\tOutput debug messages.\n"
	    "\t-h\tPrint this message.\n"
	    "\t-R\tCompute Robinson's X for specified directory.\n"
	    "\t-a age\tExclude tokens with older ages.\n"
	    "\t-c count\tExclude tokens with lower counts.\n"
	    "\t-s min,max\tExclude tokens with lengths between min and max.\n"
	    "\t-n\tReplace non-ascii characters with '?'.\n"
	    "\t-y today\tSet default day-of-year (1..366).\n"
	    "\t-V\tPrint program version.\n"
	    "%s is part of the bogofilter package.\n", PROGNAME);
}

#undef	ROBX

int main(int argc, char *argv[])
{
    typedef enum { NONE, DUMP, LOAD, WORD, MAINTAIN, ROBX } cmd_t;

    int count = 0;
    int option;
    char *db_file = NULL;
    cmd_t flag = NONE;
    bool  prob = false;

    set_today();		/* compute current date for token age */

    fpin = stdin;
    dbgout = stderr;

    directory = get_directory();

    while ((option = getopt(argc, argv, ":d:l:m:w:R:phvVx:a:c:s:ny:I:")) != -1)
	switch (option) {
	case 'd':
	    flag = DUMP;
	    count += 1;
	    db_file = (char *) optarg;
	    break;

	case 'l':
	    flag = LOAD;
	    count += 1;
	    db_file = (char *) optarg;
	    break;

	case 'm':
	    flag = MAINTAIN;
	    count += 1;
	    db_file = (char *) optarg;
	    break;

	case 'w':
	    flag = WORD;
	    count += 1;
	    db_file = (char *) optarg;
	    break;

	case 'R':
	    flag = ROBX;
	    count += 1;
	    db_file = (char *) optarg;
	    break;

	case 'p':
	    prob = true;
	    break;

	case 'v':
	    verbose++;
	    break;

	case '?':
	    help();
	    exit(2);

	case 'h':
	    help();
	    exit(0);

	case 'V':
	    print_version();
	    exit(0);

	case 'x':
	    set_debug_mask( (char *) optarg );
	    break;

	case 'a':
	    thresh_date = string_to_date((char *)optarg);
	    break;

	case 'c':
	    thresh_count = atol((char *)optarg);
	    break;

	case 's':
	    {
		unsigned long mi, ma;

		if (2 == sscanf((const char *)optarg, "%lu,%lu", &mi, &ma)) {
		    size_min = mi;
		    size_max = ma;
		} else {
		    fprintf(stderr, "syntax error in argument \"%s\" of -s.",
			    optarg);
		}
	    }
	    break;

	case 'n':
	    replace_nonascii_characters ^= true;
	    break;

	case 'y':		/* date as YYYYMMDD */
	    today = string_to_date((char *)optarg);
	    break;

	case 'I':
	    fpin = fopen( optarg, "r" );
	    if (fpin == NULL) {
		fprintf(stderr, "Can't read file '%s'\n", optarg);
		exit(2);
	    }
	    break;

	default:
	    fprintf(stderr, "Unknown option: '%c'\n", option);
	    usage();
	    exit(1);
	}

    if (count != 1)
    {
      fprintf(stderr, "%s: Exactly one of the -d, -l, or -w flags "
	      "must be present.\n", PROGNAME);
      exit(1);
    }

    /* Extra or missing parameters */
    if (flag != WORD && argc != optind) {
	usage();
	exit(1);
    }

    switch(flag) {
	case DUMP:
	    return dump_file(db_file);
	case LOAD:
	    return load_file(db_file);
	case MAINTAIN:
	    return maintain_wordlist_file(db_file);
	case WORD:
	    argc -= optind;
	    argv += optind;
	    return display_words(db_file, argc, argv, prob);
	case ROBX:
	    return compute_robinson_x(db_file);
	case NONE:
	default:
	    /* should have been handled above */
	    abort();
    }
}
