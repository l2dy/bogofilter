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

#include "bogofilter.h"
#include "datastore.h"
#include "datastore_db.h"
#include "error.h"
#include "maint.h"
#include "robinson.h"			/* for ROBS and ROBX */
#include "swap.h"

#define PROGNAME "bogoutil"

#define MINIMUM_FREQ	5		/* minimum freq */

run_t run_type = RUN_NORMAL;

const char *progname = PROGNAME;

static int dump_file(char *db_file)
{
    dbh_t *dbh;

    DBC dbc;
    DBC *dbcp;
    DBT key, data;

    int ret;
    int rv = 0;

    dbcp = &dbc;

    memset(&key, 0, sizeof(DBT));
    memset(&data, 0, sizeof(DBT));

    if ((dbh = db_open(db_file, db_file, DB_READ)) == NULL) {
	rv = 2;
    }
    else {
	db_lock_reader(dbh);

	if ((ret = dbh->dbp->cursor(dbh->dbp, NULL, &dbcp, 0) != 0)) {
	    dbh->dbp->err(dbh->dbp, ret, PROGNAME " (cursor): %s", db_file);
	    rv = 2;
	}
	else {
	    for (;;) {
		ret = dbcp->c_get(dbcp, &key, &data, DB_NEXT);
		if (ret == 0) {
		    long cv[2];
		    dbv_t val;
		    memcpy( &cv, data.data, data.size );
		    if (!dbh->is_swapped){			/* convert from struct to array */
			val.count = cv[0];
			val.date  = cv[1];
		    } else {
			val.count = swap_32bit(cv[0]);
			val.date  = swap_32bit(cv[1]);
		    }
		    if (!keep_count(val.count) || !keep_date(val.date) || !keep_size(key.size))
			continue;
		    if (replace_nonascii_characters)
			do_replace_nonascii_characters((byte *)key.data);
		    if (data.size != 4 && data.size != 8)
			print_error(__FILE__, __LINE__, "Unknown data size - %d.\n", data.size);
		    if (data.size == 4 || val.date == 0)
			printf("%.*s %lu\n", (int)key.size, (char *) key.data, val.count);
		    else
			printf("%.*s %lu %lu\n", (int)key.size, (char *) key.data, val.count, val.date);
		}
		else if (ret == DB_NOTFOUND) {
		    break;
		}
		else {
		    dbh->dbp->err(dbh->dbp, ret, PROGNAME " (c_get)");
		    rv = 2;
		    break;
		}
	    }
	}
	db_lock_release(dbh);
    }
    return rv;
}

#define BUFSIZE 512

static int load_file(char *db_file)
{
    dbh_t *dbh;
    byte buf[BUFSIZE];
    unsigned char *p;
    int rv = 0;
    size_t len;
    long line = 0;
    long count, date;
    YYYYMMDD today_save = today;

    if ((dbh = db_open(db_file, db_file, DB_WRITE)) == NULL)
	return 2;

    memset(buf, '\0', BUFSIZE);

    db_lock_writer(dbh);

    for (;;) {

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

	p = buf;
	while ( *p != '\0' && !isspace((unsigned char)*p) )
	    ++p;

	*p++ = '\0';	/* delimit token */

	while ( *p != '\0' && isspace((unsigned char)*p) )	/* skip whitespace */
	    ++p;

	count = 0;
	while (isdigit((unsigned char)*p))			/* parse count */
	    count = count * 10 + *p++ - '0';

	while ( *p != '\0' && isspace((unsigned char)*p) )	/* skip whitespace */
	    ++p;

	date = 0;
	while (isdigit((unsigned char)*p))			/* parse date */
	    date = date * 10 + *p++ - '0';
	
	while ( *p != '\0' && isspace((unsigned char)*p) )	/* skip whitespace */
	    ++p;

	if ( *p != '\0' ) {
	    fprintf(stderr,
		    "%s: Unexpected input [%s] on line %ld. "
		    "Expecting whitespace before count\n",
		    PROGNAME, buf, line);
	    rv = 1;
	    break;
	}

	if (date == 0)				/* date as YYYYMMDD */
	    date = today_save;

	if (replace_nonascii_characters)
	    do_replace_nonascii_characters(buf);

	if (!keep_count(count) || !keep_date(date) || !keep_size(strlen((const char *)buf)))
	    continue;

	/* Slower, but allows multiple lists to be concatenated */
	set_date(date);
	count += db_getvalue(dbh, (char *)buf);
	db_setvalue(dbh, (char *)buf, count);
    }
    db_lock_release(dbh);
    db_close(dbh);

    return rv;
}

static dbh_t *db_open_and_lock_file( const char *db_file, const char *name, dbmode_t mode)
{
    dbh_t *dbh = db_open(db_file, db_file, DB_READ);
    if (dbh != NULL)
	db_lock_reader(dbh);
    return dbh;
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
    dbh_t *dbh;
    int rv = 0;

    dbh = db_open_and_lock_file(db_file, db_file, DB_READ);
    if ( dbh == NULL )
	return 2;

    if ( argc == 0)
    {
	char buf[BUFSIZE];
	while (get_token(buf, BUFSIZE, stdin) == 0) {
	    printf("%s %ld\n", buf, db_getvalue(dbh, buf));
	}
    }
    else
    {
	while (argc-- > 0) {
	    char *token = *argv++;
	    printf("%s %ld\n", token, db_getvalue(dbh, token));
	}
    }

    db_close(dbh);

    return rv;
}

static int words_from_path(const char *dir, int argc, char **argv, bool show_probability)
{
    dbh_t *dbh_good;
    dbh_t *dbh_spam;
    char filepath[PATH_LEN];
    char buf[BUFSIZE];
    char *token = buf;
    long int spam_count, spam_msg_count = 0 ;
    long int good_count, good_msg_count = 0 ;

    const char *head_format = !show_probability ? "%-20s %6s %6s\n"   : "%-20s %6s  %6s  %6s  %6s\n";
    const char *data_format = !show_probability ? "%-20s %6ld %6ld\n" : "%-20s %6ld  %6ld  %f  %f\n";

    if (build_path(filepath, sizeof(filepath), dir, GOODFILE) < 0)
	return 2;

    if ((dbh_good = db_open_and_lock_file(filepath, GOODFILE, DB_READ)) == NULL)
	return 2;

    if (build_path(filepath, sizeof(filepath), dir, SPAMFILE) < 0)
	return 2;

    if ((dbh_spam = db_open_and_lock_file(filepath, SPAMFILE, DB_READ)) == NULL)
	return 2;

    if (show_probability)
    {
	spam_msg_count = db_getvalue(dbh_spam, ".MSG_COUNT"); 
	good_msg_count = db_getvalue(dbh_good, ".MSG_COUNT");
    }

    printf(head_format, "", "spam", "good", "Gra prob", "Rob prob");

    while (argc >= 0)
    {
	double gra_prob = 0.0f, rob_prob = 0.0f;
	
	if ( argc == 0)
	{
	    if (get_token(buf, BUFSIZE, stdin) != 0)
		break;
	}
	else 
	{
	    token = *argv++;
	    if (--argc == 0)
		argc = -1;
	}
	spam_count = db_getvalue(dbh_spam, token);
	good_count =  db_getvalue(dbh_good, token);
	if (show_probability)
	{
	    double spamness = (double) spam_count / (double) spam_msg_count;
	    double goodness = (double) good_count / (double) good_msg_count;

	    gra_prob = (spam_count + good_count <= MINIMUM_FREQ)
		? UNKNOWN_WORD
		: spamness / (spamness+goodness);
	    rob_prob = ((ROBS * ROBX + spamness) / (ROBS + spamness+goodness));
	}
	printf(data_format, token, spam_count, good_count, gra_prob, rob_prob);
    }

    db_close(dbh_good);
    db_close(dbh_spam);

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
    }
    else
    {
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

static double compute_robx(dbh_t *dbh_spam, dbh_t *dbh_good)
{
    DBT    key, data;
    int    ret = 0;
    int    word_cnt = 0;
    double sum = 0.0;
    double robx;

    DBC  dbc_spam;
    DBC *dbcp_spam = &dbc_spam;
    DBC  dbc_good;
    DBC *dbcp_good = &dbc_good;

    double scalefactor;
    long int msg_good, msg_spam;

    msg_good = db_getvalue( dbh_good, ".MSG_COUNT" );
    msg_spam = db_getvalue( dbh_spam, ".MSG_COUNT" );
    scalefactor = (double) msg_spam / (double) msg_good;

    memset(&key, 0, sizeof(DBT));
    memset(&data, 0, sizeof(DBT));

    if ((ret = dbh_good->dbp->cursor(dbh_good->dbp, NULL, &dbcp_good, 0) != 0)) {
	dbh_good->dbp->err(dbh_good->dbp, ret, PROGNAME " (cursor): %s", "dbh_good->file");
	return 2;
    }

    for (;;) {
	ret = dbcp_good->c_get(dbcp_good, &key, &data, DB_NEXT);
	if (ret == 0) {
	    word_cnt += 1;		/* count words in good list */
	}
	else if (ret == DB_NOTFOUND) {
	    break;
	}
	else {
	    dbh_good->dbp->err(dbh_spam->dbp, ret, PROGNAME " (c_get)");
	    ret = 2;
	    break;
	}
    }
    dbcp_good->c_close(dbcp_good);

    if ((ret = dbh_spam->dbp->cursor(dbh_spam->dbp, NULL, &dbcp_spam, 0) != 0)) {
	dbh_spam->dbp->err(dbh_spam->dbp, ret, PROGNAME " (cursor): %s", "dbh_spam->file");
	return 2;
    }

    for (;;) {
	ret = dbcp_spam->c_get(dbcp_spam, &key, &data, DB_NEXT);
	if (ret == 0) {
	    long goodness;
	    unsigned long spamness;
	    double        prob;
	    char         *token = key.data;
	    token[key.size] = '\0';

	    /* ignore system meta-data */
	    if ( *token == '.')
		continue;

	    spamness = *(unsigned long *) data.data;
	    goodness = db_getvalue(dbh_good, token);

	    prob = spamness / (goodness*scalefactor + spamness);
	    sum += prob;

	    /* tokens in good list were already counted */
	    /* now add in tokens only in spam list */
	    if (goodness == 0)
		word_cnt += 1;

	    /* print if token in both word lists */
	    if (verbose > 1 && (goodness && spamness))
		printf("cnt: %6d,  sum: %12.6f,  ratio: %f,  sp: %4lu,  gd: %4ld,  p: %f,  t: %s\n", word_cnt, sum, sum/word_cnt, spamness, goodness, prob, token);
	}
	else if (ret == DB_NOTFOUND) {
	    break;
	}
	else {
	    dbh_spam->dbp->err(dbh_spam->dbp, ret, PROGNAME " (c_get)");
	    ret = 2;
	    break;
	}
    }
    dbcp_spam->c_close(dbcp_spam);

    robx = sum/word_cnt;
    if (verbose)
	printf( ".MSG_COUNT: %ld, %ld, scale: %f, sum: %f, cnt: %6d, .ROBX: %f\n",
		msg_spam, msg_good, scalefactor, sum, (int)word_cnt, robx);

    return robx;
}

static int compute_robinson_x(char *path)
{
    int e;

    dbh_t *dbh_good;
    dbh_t *dbh_spam;

    char db_spam_file[PATH_LEN];
    char db_good_file[PATH_LEN];

    double robx;

    e = build_path(db_spam_file, sizeof(db_spam_file), path, "spamlist.db");
    if (e < 0) goto overflow;
    e = build_path(db_good_file, sizeof(db_good_file), path, "goodlist.db");
    if (e < 0) goto overflow;

    dbh_good = db_open(db_good_file, "good", DB_READ);
    dbh_spam = db_open(db_spam_file, "spam", DB_WRITE);

    db_lock_reader(dbh_good);
    db_lock_writer(dbh_spam);

    robx = compute_robx( dbh_spam, dbh_good );

    db_setvalue(dbh_spam, ".ROBX", (int) (robx * 1000000));

    db_lock_release(dbh_spam);
    db_close(dbh_spam);

    db_lock_release(dbh_good);
    db_close(dbh_good);

    return 0;
overflow:
    fprintf(stderr, "%s: string too long creating .db file name.\n", PROGNAME);
    exit(2);
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

    while ((option = getopt(argc, argv, "d:l:m:w:R:phvVx:a:c:s:ny:I:")) != -1)
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
	    sscanf((char *)optarg, "%d,%d", &size_min, &size_max);
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
