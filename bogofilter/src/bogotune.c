/* $Id$ */

/*****************************************************************************

NAME:
   bogotune.c -- determine optimal parameters for bogofilter

AUTHOR:
   David Relson - C version
   Greg Lous - perl version

******************************************************************************/

/* To allow tuning of large corpora, the memory use must be minimized
** for each messages.  Given the wordlist in ram, foreach token of a
** test message bogotune only needs to know its spam and ham counts.
**
** 1. external wordlist ("-d") flag
**    a. read wordlist.db
**    b. read messages for test set
**       1. lookup words in wordlist
**       2. discard words
**    c. replace wordhashs with wordcnts
**    d. de-allocate resident wordlist
** 
** 2. internal wordlist ("-D") flag
**    a. read all messages
**    b. distribute messages
**        1. proper pct to wordlist
**        2. proper pct to test set
** 	  2a. create wordprops from wordlists
**    c. replace wordprops with wordcnts
**    d. de-allocate resident wordlist
*/

/* Limitations:
**
**	If all the input messages are in msg-count format,
**	bogotune will use default ROBX value since an external
**	wordlist is needed to compute a real robx value.
*/

#include "common.h"

#include <math.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "bogotune.h"
#include "bogoreader.h"
#include "collect.h"
#include "datastore.h"
#include "fisher.h"
#include "msgcounts.h"
#include "mime.h"
#include "paths.h"
#include "rstats.h"
#include "token.h"
#include "tunelist.h"
#include "wordhash.h"
#include "xmalloc.h"
#include "xstrdup.h"

#define	MSG_COUNT	".MSG_COUNT"

#define	TEST_COUNT	500	/* minimum allowable message count */
#define	PREF_COUNT	4000	/* preferred message count */
#define	LIST_COUNT	2000	/* minimum msg count in wordlist */

#define	HAM_CUTOFF	0.10
#define	MIN_CUTOFF	0.52	/* minimum for get_thresh() */
#define	MAX_CUTOFF	0.99	/* maximum for get_thresh() */
#define	SPAM_CUTOFF	0.95
#define FP_CUTOFF	0.999

enum e_verbosity {
    SUMMARY	   = 1,	/* summarize main loop iterations */
    PARMS	   = 2,	/* print parameter sets (rs, md, rx) */
    SCORE_SUMMARY  = 4,	/* verbosity level for printing scores */
    SCORE_DETAIL	/* verbosity level for printing scores */
};

#define	MOD(n,m)	((n) - ((int)((n)/(m)))*(m))

#define	MIN(n)		((n)/60)
#define SECONDS(n)	((n) - MIN(n)*60)

#define	ROUND(f)	floor(f+0.5)

/* Global Variables */

const char *progname = "bogotune";
char *ds_file;

uint target = 0;
uint memdebug = 0;
uint max_messages_per_mailbox = 0;

extern double robx, robs;

word_t *w_msg_count;
wordhash_t *t_ns, *t_sp;

wordlist_t *ns_and_sp;
wordlist_t *ns_msglists, *sp_msglists;

flhead_t *spam_files, *ham_files;

#ifdef	ENABLE_MEMDEBUG
extern uint32_t dbg_trap_index;
#endif

typedef struct {
    uint cnt;
    double *data;
} data_t;

data_t *rsval;
data_t *rxval;
data_t *mdval;
uint ns_cnt, sp_cnt;

double *ns_scores;
double *sp_scores;
double user_robx = 0.0;		/* from '-r' option */

/* Function Prototypes */

static void bt_exit(void);
static int ds_read_hook(word_t *key, dsv_t *data, void *userdata);
static void show_elapsed_time(int beg, int end, uint cnt, double val, 
			      const char *lbl1, const char *lbl2);

/* Function Definitions */

static data_t *seq_by_cnt(double fst, double lst, int cnt)
{
    int i;
    data_t *val = xcalloc(1, sizeof(data_t));

    val->cnt = cnt;
    val->data = xcalloc(cnt, sizeof(double));
    for (i=0; i < cnt; i += 1)
	val->data[i] = fst + i * (lst - fst) / (cnt - 1);

    return val;
}

static int get_cnt(double fst, double lst, double amt)
{
    int cnt = (fabs(lst - fst) + EPS) / (fabs(amt) - EPS) + 1;
    return cnt;
}

static data_t *seq_by_amt(double fst, double lst, double amt)
{
    uint i;
    data_t *val = xcalloc(1, sizeof(data_t));

    val->cnt = get_cnt(fst, lst, amt);
    val->data = xcalloc(val->cnt, sizeof(double));

    for (i=0; i < val->cnt; i += 1)
	val->data[i] = fst + i * amt;

    return val;
}

static data_t *seq_by_pow(double fst, double lst, double amt)
{
    uint i;
    data_t *val = xcalloc(1, sizeof(data_t));

    val->cnt = get_cnt(fst, lst, amt);
    val->data = xcalloc(val->cnt, sizeof(double));

    for (i=0; i < val->cnt; i += 1)
	val->data[i] = pow(10, fst + i * amt);

    return val;
}

static void data_free(data_t *val)
{
    xfree(val->data); xfree(val);
}

static void init_coarse(double _rx)
{
#ifndef	DUP_PERL
    rxval = seq_by_cnt(_rx - 0.1, _rx + 0.1, 5);
#else
    /* special order required by outlier check  */
    double x0, x1;
    x0 = _rx - 0.1;
    x1 = _rx + 0.1;
    rxval->data[0] = x0 + (x1 - x0) / 2;
    rxval->data[1] = x0 + (x1 - x0) / 4;
    rxval->data[2] = x0 + (x1 - x0) * 3 / 4;
    rxval->data[3] = x0;
    rxval->data[4] = x1;
#endif
    rsval = seq_by_pow(0.0, -2.0, -0.5);
    mdval = seq_by_cnt(0.05, 0.45, 9);
}

static void init_fine(double _rs, double _md, double _rx)
{
    double s0, s1;

    s0 = log10(_rs) - 0.5;
    s1 = log10(_rs) + 0.5;
    if (s0 < -2.0) s0 = -2.0;
    if (s1 >  0.0) s1 =  0.0;

    rsval = seq_by_pow(s1, s0, -0.25);

    s0 = _md - 0.075;
    s1 = _md + 0.075;
    if (s0 < 0.02 ) s0 = 0.02;
    if (s1 > 0.465) s1 = 0.465;

    mdval = seq_by_amt(s0, s1, 0.015);

    s0 = _rx - 2 * 0.02;
    s1 = _rx + 2 * 0.02;
#ifndef	DUP_PERL
    rxval = seq_by_amt(s0, s1, 0.02);
#else
    /* special order required by outlier check  */
    rxval->data[0] = _rx;
    rxval->data[1] = _rx - 0.02;
    rxval->data[2] = _rx + 0.02;
    rxval->data[3] = _rx - 0.04;
    rxval->data[4] = _rx + 0.04;
#endif
}

static void print_parms(const char *label, const char *format, data_t *data)
{
    uint i;
    printf("  %s: %2d ", label, data->cnt);
    for (i = 0; i < data->cnt; i += 1) {
	printf("%s", (i==0) ? " (" : ", ");
	printf(format, data->data[i]); /* RATS: ignore */
    }
    printf(")\n"); fflush(stdout);
}

static void print_all_parms(void)
{
    print_parms("rsval", "%6.4f", rsval);
    print_parms("rxval", "%5.3f", rxval);
    print_parms("mdval", "%5.3f", mdval);
}

static int compare_double(const void *const ir1, const void *const ir2)
{
    const double d1 = *(const double *)ir1;
    const double d2 = *(const double *)ir2;

    if (d1 - d2 > 0.0) return  1;
    if (d2 - d1 > 0.0) return -1;

    return 0;
}

static int compare_results(const void *const ir1, const void *const ir2)
{
    result_t const *r1 = (result_t const *)ir1;
    result_t const *r2 = (result_t const *)ir2;

    if (r1->fn > r2->fn ) return  1;
    if (r2->fn > r1->fn ) return -1;

    if (r1->idx > r2->idx ) return  1;
    if (r2->idx > r1->idx ) return -1;

    return 0;
}

/* Score all non-spam */

static void score_ns(double *results)
{
    uint i;
    uint count = 0;

    if (verbose >= SCORE_DETAIL)
	printf("ns:\n");

    for (i=0; i < COUNTOF(ns_msglists->u.sets); i += 1) {
	mlhead_t *list = ns_msglists->u.sets[i];
	mlitem_t *item;
	for (item = list->head; item != NULL; item = item->next) {
	    wordhash_t *wh = item->wh;
	    double score = (*method->compute_spamicity)(wh, NULL);
	    results[count++] = score;
	    if (verbose >= SCORE_DETAIL)
		printf("%6d %0.16f\n", count-1, score);
	}
    }

    return;
}

static bool check_for_high_ns_scores(void)
{
    double percent = 0.0025;
    target = ceil(ns_cnt * percent);

    score_ns(ns_scores);	/* scores in ascending order */
    qsort(ns_scores, ns_cnt, sizeof(double), compare_double);

    if (ns_scores[ns_cnt-target] < SPAM_CUTOFF)
	return false;
    else {
	fprintf(stderr, "Warning:  high scoring non-spam.\n");
	return true;
    }
}

/* Score all spam to determine false negative counts */

static void score_sp(double *results)
{
    uint i;
    uint count = 0;

    if (verbose >= SCORE_DETAIL)
	printf("sp:\n");

    for (i=0; i < COUNTOF(sp_msglists->u.sets); i += 1) {
	mlhead_t *list = sp_msglists->u.sets[i];
	mlitem_t *item;
	for (item = list->head; item != NULL; item = item->next) {
	    wordhash_t *wh = item->wh;
	    double score = (*method->compute_spamicity)(wh, NULL);
	    results[count++] = score;
	    if (verbose >= SCORE_DETAIL)
		printf("%6d %0.16f\n", count-1, score);
	}
    }

    return;
}

static uint get_fn_count(uint count, double *results)
{
    uint i;
    uint fn = 0;

    for (i=0; i < count; i += 1) {
	double score = results[i];
	if (score < spam_cutoff)
	    fn += 1;
    }

    return fn;
}

static bool check_for_low_sp_scores(void)
{
    double percent = 0.0025;
    target = ceil(sp_cnt * percent);

    score_sp(sp_scores);	/* scores in ascending order */
    qsort(sp_scores, sp_cnt, sizeof(double), compare_double);

    if (sp_scores[target] > HAM_CUTOFF)
	return false;
    else {
	fprintf(stderr, "Warning:  low scoring spam.\n");
	return true;
    }
}

static void scoring_error(void)
{
    int i;

    printf("    high ham scores:\n");
    for (i = 0; i < 10 && ns_scores[ns_cnt-i-1] > SPAM_CUTOFF; i += 1) 
	printf("      %2d %8.6f\n", i+1, ns_scores[ns_cnt-i-1]);

    printf("    low spam scores:\n");
    for (i = 0; i < 10 && sp_scores[i] < HAM_CUTOFF; i += 1) 
	printf("      %2d %8.6f\n", i+1, sp_scores[i]);
}

static double scale(uint cnt, uint lo, uint hi, double beg, double end)
{
    double ans;

    if (cnt < lo)
	return beg;
    if (cnt > hi)
	return end;

    ans = beg + (end - beg) * (cnt - lo ) / (hi - lo);

    return ans;
}

/* compute scores and get fp target and spam cutoff value
**
** As count increases from 500 to 4000 ...
**	1) initial target percent drops from 1% to 0.25% 
**	2) initial minimum target increases from 5 to 10
*/

static uint get_thresh(uint count, double *scores)
{
    uint   ftarget = 0;
    uint   mtarget = scale(count, TEST_COUNT, PREF_COUNT, 3, 10);
    double percent = scale(count, TEST_COUNT, PREF_COUNT, 0.01, 0.0025);
    double cutoff;
    static bool show_first = false;

    score_ns(scores);			/* scores in ascending order */
    qsort(scores, count, sizeof(double), compare_double);

    while (percent > 0.0001) {
	ftarget = ceil(count * percent);	/* compute fp count */
	if (ftarget <= mtarget)
	    break;
	cutoff = scores[count - ftarget];	/* get cutoff value */
	if (cutoff >= MIN_CUTOFF)
	    break;
	percent -= 0.0001;
    }

    if (verbose >= PARMS)
	printf("mtarget %d, ftarget %d, percent %6.4f%%\n", mtarget, ftarget, percent * 100.0);

    if (show_first) {
	uint f;
	show_first = false;
	for (f = 1; f <= ftarget; f += 1) 
	    printf("%2d %5d %8.6f\n", f, count-f, scores[count-f]);
    }

    if (!force && (cutoff < MIN_CUTOFF || cutoff > MAX_CUTOFF)) {
	fprintf(stderr,
		"%s high-scoring non-spams in this data set.\n",
		(cutoff < MIN_CUTOFF) ? "Too few" : "Too many");
	fprintf(stderr,
		"At target %d, cutoff is %8.6f.\n", ftarget, cutoff);
	exit(EX_ERROR);
    }

    /* ensure cutoff is below SPAM_CUTOFF */
    while (cutoff >= SPAM_CUTOFF && ++ftarget < count)
	cutoff = scores[count-ftarget];

    spam_cutoff = scores[count-(ftarget+1)] + 0.0005;

    return ftarget;
}

static uint read_mailbox(char *arg)
{
    uint count = 0;
    wordhash_t *train = ns_and_sp->train;
    wordlist_t *ns_or_sp = (run_type == REG_GOOD) ? ns_msglists : sp_msglists;

    if (verbose) {
	printf("Reading %s\n", arg);
	fflush(stdout);
    }

    mbox_mode = true;
    bogoreader_init(1, &arg);
    while ((*reader_more)()) {
	wordhash_t *whc, *whp;

	whc = wordhash_new();
	collect_words(whc);

	whp = convert_wordhash_to_propslist(whc, train);
	msglist_add(ns_or_sp->msgs, whp);

	wordhash_free(whc);

	if (verbose && (count % 100) == 0) {
	    if ((count % 1000) != 0)
		putchar('.');
	    else
		printf("\r              \r%d ", count/1000 );
	    fflush(stdout);
	}

	count += 1;

	if (max_messages_per_mailbox != 0 &&
	    count > max_messages_per_mailbox)
	    break;
    }

    ns_and_sp->count += count;
    bogoreader_fini();

    if (verbose) {
	printf("\r              \r%d messages\n", count);
	fflush(stdout);
    }

    return count;
}

static uint filelist_read(int mode, flhead_t *list)
{
    uint count = 0;
    flitem_t *item;
    run_type = mode;
    for (item = list->head; item != NULL; item = item->next) {
	lexer = NULL;
	msg_count_file = false;
	count += read_mailbox(item->name);
    }
    return count;
}

/* distribute()
**
**	Proportionally distribute messages between training and scoring sets.
**
**   Method:
**	If only 2500 messages, use 2000 for training and 500 for scoring.
**	If over 4000 messages, use equal numbers for training and scoring.
**	In between 2500 and 4000, do a proportional distribution.
*/

static void distribute(int mode, wordlist_t *ns_or_sp)
{
    int good = mode == REG_GOOD;
    int bad  = 1 - good;

    wordhash_t *train = ns_and_sp->train;
    mlhead_t *list = ns_or_sp->msgs;
    mlitem_t *item;
    wordprop_t *msg_count;

    int score_count = 0;
    int train_count = 0;

    double ratio;

    if (ds_file != NULL)
	ratio = 0.0;
    else {
	int count = list->count;
	const double small_count = LIST_COUNT + TEST_COUNT;
	const double large_count = LIST_COUNT + LIST_COUNT;
	const double small_ratio = LIST_COUNT / TEST_COUNT;
	const double large_ratio = LIST_COUNT / LIST_COUNT;
	if (count > large_count)
	    count = large_count;
	if (count < small_count)
	    count = small_count;
	ratio = small_ratio + (large_ratio - small_ratio) * (count - small_count) / (large_count - small_count);
    }
    
    /* Update .MSG_COUNT */
    msg_count = wordhash_insert(train, w_msg_count, sizeof(wordprop_t), &wordprop_init);

    for (item = list->head; item != NULL; item = item->next) {
	wordhash_t *wh = item->wh;

	/* training set */
	if ((ds_file == NULL) && (train_count / ratio < score_count + 1)) {
	    wordhash_set_counts(wh, good, bad);
	    wordhash_add(train, wh, &wordprop_init);
	    train_count += 1;
	    msg_count->cnts.good += good;
	    msg_count->cnts.bad  += bad;
	    wordhash_free(wh);
	}
	/* scoring set  */
	else {
	    msglist_add(ns_or_sp->u.sets[MOD(score_count,3)], wh);
	    score_count += 1;
	}
	item->wh = NULL;
    }

    return;
}

static void create_countlists(wordlist_t *ns_or_sp)
{
    uint i;
    uint c = COUNTOF(ns_or_sp->u.sets);
    for (i=0; i<c; i += 1) {
	mlhead_t *list = ns_or_sp->u.sets[i];
	mlitem_t *item;

	for (item = list->head; item != NULL; item = item->next) {
	    wordhash_t *who = item->wh;
	    wordhash_t *whn = convert_propslist_to_countlist(who);
	    wordhash_free(who);
	    item->wh = whn;
	}
    }

    return;
}

static void usage(void)
{
    (void)fprintf(stderr, 
		  "Usage: %s [ -C | -D | -r | -h | -v | -F ] { -c config } { -d directory } -n non-spam-files -s spam-files\n", 
		  progname);
}

static void help(void)
{
    usage();
    (void)fprintf(stderr,
		  "\t  -h      - print this help message.\n"
		  "\t  -C      - don't read standard config files.\n"
		  "\t  -c file - read specified config file.\n"
		  "\t  -D      - don't read a wordlist file.\n"
		  "\t  -d path - specify directory for wordlists.\n"
		  "\t  -r      - specify robx value\n"
		  "\t  -s file1 file2 ... - spam files\n"
		  "\t  -n file1 file2 ... - non-spam files\n" 
		  "\t  -v      - increase level of verbose messages\n"
		  "\t  -F      - accept initial spam_cutoff < 0.5 and\n"
		  "\t          - accept high scoring non-spam and low scoring spam\n"
	);
    (void)fprintf(stderr,
		  "\n"
		  "%s (version %s) is part of the bogofilter package.\n", 
		  progname, version);
}

static int process_args(int argc, char **argv)
{
    int  count = 1;

    bulk_mode = B_CMDLINE;

    while (--argc > 0) {
	char *arg = *++argv;
	count += 1;
	if (*arg == '-') {
	    while (*++arg) {
		switch (*arg) {
		case 'c':
		    argc -= 1;
		    read_config_file(*++argv, false, !quiet, PR_CFG_USER);
		case 'C':
		    suppress_config_file = true;
		    break;
		case 'd':
		    argc -= 1;
		    ds_file = *++argv;
		    break;
		case 'D':
		    ds_file = NULL;
		    break;
		case 'F':	/* accept initial spam_cutoff < 0.5 and
				** accept high scoring non-spam and low scoring spam
				*/
		    force = true;
		    break;
		case 'n':
		    run_type = REG_GOOD;
		    break;
		case 'r':
		    argc -= 1;
		    user_robx = atof(*++argv);
		    break;
		case 's':
		    run_type = REG_SPAM;
		    break;
		case 'v':
		    verbose += 1;
		    break;
#ifdef	ENABLE_MEMDEBUG
		case 'm':
		    memtrace += 1;
		    break;
		case 'M': 
		    memdebug += 1;
		    if ( argv[1][0] != '-') {
			argc -= 1;
			dbg_trap_index=atoi(*++argv);
		    }
		    break;
#endif
		default:
		    help();
		    exit(EX_ERROR);
		}
	    }
	}
	else
	    filelist_add( (run_type == REG_GOOD) ? ham_files : spam_files, arg);
    }

    if (spam_files->count == 0 || ham_files->count == 0) {
	fprintf(stderr, 
		"Bogotune needs both non-spam and spam messages sets for its testing.\n");
	exit(EX_ERROR);
    }

    return count;
}

static void load_wordlist(char *file)
{
    struct stat sb;
    size_t len = strlen(file) + strlen(WORDLIST) + 2;
    char *path = xmalloc(len);

    if (stat(file, &sb) != 0) {
	fprintf(stderr, "Error accessing file or directory '%s'.\n", ds_file);
	if (errno != 0)
	    fprintf(stderr, "error #%d - %s.\n", errno, strerror(errno));
	return;
    }

    build_path(path, len, file, WORDLIST);

    if (verbose) {
	printf("Reading %s\n", path);
	fflush(stdout);
    }

    ds_oper(path, DB_READ, ds_read_hook, ns_and_sp->train);
    db_cachesize = ceil(sb.st_size / 3.0 / 1024.0 / 1024.0);

    if (path != ds_file)
	xfree(path);

    return;
}

static int ds_read_hook(word_t *key, dsv_t *data, void *userdata)
/* returns 0 if ok, 1 if not ok */
{
    wordhash_t *train = userdata;

    wordprop_t *tokenprop = wordhash_insert(train, key, sizeof(wordprop_t), &wordprop_init);
    tokenprop->cnts.bad = data->spamcount;
    tokenprop->cnts.good = data->goodcount;

    return 0;
}

typedef struct robhook_data {
    double   sum;
    int      count;
    double   scalefactor;
} rh_t;

static void robx_accum(word_t *key,
		       void   *data,
		       void   *userdata)
{
    rh_t *rh = userdata;
    wordprop_t *wp = data;
    uint goodness = wp->cnts.good;
    uint spamness = wp->cnts.bad;
    double prob = spamness / (goodness * rh->scalefactor + spamness);
    bool doit = goodness + spamness >= 10;

    /* ignore system meta-data */
    if (*key->text == '.')
	return;

    if (doit) {
	rh->sum += prob;
	rh->count += 1;
    }

    return;
}

static double robx_compute(wordhash_t *wh)
{
    double rx;
    wordprop_t *p = wordhash_search(wh, w_msg_count, 0);
    uint msg_good = p->cnts.good;
    uint msg_spam = p->cnts.bad;

    rh_t rh;
    rh.scalefactor = (double)msg_spam/(double)msg_good;
    rh.sum = 0.0;
    rh.count = 0;

    wordhash_foreach(wh, robx_accum, &rh);

    if (rh.count == 0)
	rx = ROBX;
    else
	rx = rh.sum/rh.count;

    if (verbose > 1) {
	printf("%s: %lu, %lu\n",
	       MSG_COUNT, (unsigned long)msg_spam, (unsigned long)msg_good);
	printf("scale: %f, sum: %f, cnt: %6d\n",
	       rh.scalefactor, rh.sum, (int)rh.count);
    }

    return rx;
}

static double robx_init(void)
{
    double x;
    printf("Calculating initial x value...\n");
    x = robx_compute(ns_and_sp->train);
    if (x > 0.6) x = 0.6;
    if (x < 0.4) x = 0.4;
    return x;
}

static result_t *results_sort(uint r_count, result_t *results)
{
    result_t *ans = xcalloc(r_count, sizeof(result_t));
    memcpy(ans, results, r_count * sizeof(result_t));
    qsort(ans, r_count, sizeof(result_t), compare_results);
    return ans;
}

static void top_ten(result_t *sorted)
{
    uint i;

    printf("Top ten parameter sets from this scan:\n");

    if (verbose)
	printf("    ");
    printf("   rs     md    rx    co      %%fp     %%fn");
    if (verbose>1)
	printf("    fp  fn");
    printf( "\n" );

    for (i=0; i < 10; i += 1) {
	result_t *r = &sorted[i];
	if (verbose)
	    printf("%3d  ", r->idx);
	printf( "%6.4f %5.3f %5.3f %6.4f  %6.4f  %6.4f",
		r->rs, r->md, r->rx, r->co, 
		r->fp*100.0/ns_cnt, r->fn*100.0/sp_cnt);
	if (verbose>1)
	    printf( "  %3d %3d", r->fp, r->fn);
	printf( "\n" );
    }

    printf("\n");
    fflush(stdout);

    return;
}

/* get false negative */

static int gfn(result_t *results, uint rsi, uint mdi, uint rxi)
{
    uint i = (rsi * mdval->cnt + mdi) * rxval->cnt + rxi;
    result_t *r = &results[i];
    int fn = r->fn;
    if (verbose > 100)
	printf("   %d, %d, %d, %2d\n", rsi, mdi, rxi, fn);
    return fn;
}

static result_t *count_outliers(uint r_count, result_t *sorted, result_t *unsorted)
{
    bool f = false;
    uint i, o = 0;
    uint fn;
    uint rsi, mdi, rxi;
    uint rsc = rsval->cnt - 1;
    uint rxc = rxval->cnt - 1;
    uint mdc = mdval->cnt - 1;
    result_t *r = &sorted[r_count / 4];
    uint mfn = r->fn;			/* median false negative */

    if (verbose)
	printf("25%% fn count was %d\n", mfn);

    for (i = 0; i < r_count; i += 1) {
	r = &sorted[i];
	rsi = r->rsi; mdi = r->mdi; rxi = r->rxi;

	if (((rsi == 0   ||
	      (fn = gfn(unsorted, rsi-1, mdi, rxi)) < mfn)) &&
	    ((rsi == rsc ||
	      (fn = gfn(unsorted, rsi+1, mdi, rxi)) < mfn)) &&
	    ((mdi == 0   ||
	      (fn = gfn(unsorted, rsi, mdi-1, rxi)) < mfn)) &&
	    ((mdi == mdc ||
	      (fn = gfn(unsorted, rsi, mdi+1, rxi)) < mfn)) &&
	    ((rxi == 0   ||
	      (fn = gfn(unsorted, rsi, mdi, rxi-1)) < mfn)) &&
	    ((rxi == rxc ||
	      (fn = gfn(unsorted, rsi, mdi, rxi+1)) < mfn)))
	{
	    f = true;
	    break;
	}
	o = i+1;
    }

    if (o > 0) {
	printf("%d outlier%s encountered.                                                   \n",
	       o, (o > 1) ? "s" : "");
    }

    if (!f) {
	r = &sorted[0];
	rsi = r->rsi; mdi = r->mdi; rxi = r->rxi;
	printf("No smooth minimum encountered, using lowest fn count (an outlier).         \n");
    }

    return r;
}

static void progress(uint cur, uint top)
{
    uint i;
    uint ndots = ceil(70.0 * cur / top);
    if (ndots < 1)
	ndots = 1;
     printf("\r[");
     for (i=0; i < ndots; i += 1)
	 printf(".");
     for (i=ndots; i < 70; i += 1)
	 printf(" ");
     printf("]");
}

static void final_recommendations(void)
{
    uint m, s;
    bool printed = false;
    const char *comment= "";
    uint minn[] = { 10000, 2000, 1000, 500, 1 };

    printf("Performing final scoring:\n");

    printf("Non-Spam...\n");
    score_ns(ns_scores);		/* get scores */
    /* ... ascending */
    qsort(ns_scores, ns_cnt, sizeof(double), compare_double);

    printf("Spam...\n");
    score_sp(sp_scores);		/* get scores */
    /* ... ascending */
    qsort(sp_scores, sp_cnt, sizeof(double), compare_double);

    printf("Recommendations:\n\n");
    printf("---cut---\n");
    printf("db_cachesize=%d\n", db_cachesize);

    printf("robx=%5.3f\n", robx);
    printf("min_dev=%5.3f\n", min_dev);
    printf("robs=%6.4f\n", robs);

    for (m=0; m < COUNTOF(minn); m += 1) {
	double cutoff;
	uint i, fp = 0, fn = 0;
	uint mn = minn[m];

	if (ns_cnt < mn)
	    continue;

	if (mn > 1 ) {
	    uint t = (ns_cnt + mn - 1) / mn;
	    cutoff = ns_scores[ns_cnt-t];
	    if (cutoff > FP_CUTOFF)
		continue;
	}
	else {
	    cutoff = SPAM_CUTOFF;
	    if (printed)
		break;
	    for (i = 0; i < ns_cnt && ns_scores[i] < cutoff; i += 1)
		fp = i;
	    if (fp == 0) {
		fprintf(stderr, 
			"too few low scoring spam.\n");
		exit(EX_ERROR);
	    }
	    fp -= 1;
	    cutoff = ns_scores[fp];
	    fp = ns_cnt - fp;
	}

	for (i = 0; i < sp_cnt; i += 1) {
	    if (sp_scores[i] >= cutoff) {
		fn = i;
		break;
	    }
	}

	printf("%sspam_cutoff=%5.3f\t# for %4.2f%% false positives; expect %4.2f%% false neg.\n",
	       comment, cutoff,
	       (mn != 1) ? 100.0 / mn : 100.0 * fp / ns_cnt,
	       100.0 * fn / sp_cnt);

	if (verbose >= PARMS)
	    printf("# mn %5d, ns_cnt %5d, sp_cnt %5d, fp %3d, fn %3d\n",
		   mn, ns_cnt, sp_cnt, fp, fn);

	comment = "#";
	printed = true;
    }

    s = ceil(sp_cnt * 0.002 - 1);
    ham_cutoff = sp_scores[s];
    if (ham_cutoff < 0.10) ham_cutoff = 0.10;
    if (ham_cutoff > 0.45) ham_cutoff = 0.45;
    printf("ham_cutoff=%5.3f\n", ham_cutoff);
    printf("---cut---\n");
    printf("\n");
    printf("Tuning completed.\n");
}

static void bogotune_init(void)
{
    const char *msg_count = MSG_COUNT;
    w_msg_count = word_new((const byte *)msg_count, strlen(msg_count));
    ns_and_sp = wordlist_new("tr");		/* training wordlist */
    ns_msglists = wordlist_new("ns");		/* non-spam scoring wordlist */
    sp_msglists = wordlist_new("sp");		/* spam scoring wordlist */

    return;
}

static void bogotune_free(void)
{
    xfree(ns_scores);
    xfree(sp_scores);

    filelist_free(ham_files);
    filelist_free(spam_files);

    wordlist_free(ns_msglists);
    wordlist_free(sp_msglists);
    wordlist_free(ns_and_sp);

    word_free(w_msg_count);

    token_cleanup();
    mime_cleanup();

    return;
}

static void get_msg_counts(void)
{
    wordprop_t *props = wordhash_insert(ns_and_sp->train, w_msg_count, sizeof(wordprop_t), &wordprop_init);
    msgs_good = props->cnts.good;
    msgs_bad  = props->cnts.bad;

    if (msgs_good < LIST_COUNT || msgs_bad < LIST_COUNT) {
	fprintf(stderr, 
		"The wordlist contains %u non-spam and %u spam messages.\n"
		"Bogotune must be run with at least %d of each.\n",
		msgs_good, msgs_bad, LIST_COUNT);
	exit(EX_ERROR);
    }

    if (msgs_bad < msgs_good / 5 ||
	msgs_bad > msgs_good * 5) {
	fprintf(stderr,
		"The wordlist has a ratio of spam to non-spam of %0.1f to 1.0.\n"
		"Bogotune requires the ratio be in the range of 0.2 to 5.\n",
		msgs_bad * 1.0 / msgs_good);
	exit(EX_ERROR);
    }

    return;
}

static rc_t bogotune(void)
{
    int beg, end;
    uint cnt, scan;
    rc_t status = RC_OK;

    bogotune_init();

    beg = time(NULL);

    if (ds_file)
	load_wordlist(ds_file);

    ham_cutoff = 0.0;
    spam_cutoff = 0.1;

    if (memdebug) { MEMDISPLAY; }

    /* Note: memory usage highest while reading messages */
    /* usage decreases as distribute() converts to count format */

    /* read all messages, merge training sets, look up scoring sets */
    ns_cnt = filelist_read(REG_GOOD, ham_files);
    sp_cnt = filelist_read(REG_SPAM, spam_files);

    if (verbose) {
	end = time(NULL);
	cnt = ns_cnt + sp_cnt;
	show_elapsed_time(beg, end, ns_cnt + sp_cnt, (double)cnt/(end-beg), "messages", "msg/sec");
    }

    if (memdebug) { MEMDISPLAY; }

    distribute(REG_GOOD, ns_msglists);
    distribute(REG_SPAM, sp_msglists);

    if (memdebug) { MEMDISPLAY; }

    create_countlists(ns_msglists);
    create_countlists(sp_msglists);

    if (memdebug) { MEMDISPLAY; }

    if (verbose && time(NULL) - end > 2) {
	end = time(NULL);
	cnt = ns_cnt + sp_cnt;
	show_elapsed_time(beg, end, ns_cnt + sp_cnt, (double)cnt/(end-beg), "messages", "msg/sec");
    }

    if (msgs_good == 0 && msgs_bad == 0) {
	get_msg_counts();
    }

    if (verbose > 3) {
	wordlist_print(ns_and_sp);
	wordlist_print(ns_msglists);
	wordlist_print(sp_msglists);
    }

    ns_cnt = count_messages(ns_msglists);
    sp_cnt = count_messages(sp_msglists);

    if (!force && (ns_cnt < TEST_COUNT || sp_cnt < TEST_COUNT)) {
	fprintf(stderr, 
		"The messages sets contain %u non-spam and %u spam.  Bogotune "
		"requires at least %d non-spam and %d spam messages to run.\n",
		ns_cnt, sp_cnt, TEST_COUNT, TEST_COUNT);
	exit(EX_ERROR);
    }

    fflush(stdout);

    ns_scores = xcalloc(ns_cnt, sizeof(double));
    sp_scores = xcalloc(sp_cnt, sizeof(double));

    method = (method_t *) &rf_fisher_method;

    robs = ROBS;
    robx = ROBX;
    min_dev = FISHER_MIN_DEV;

    if (check_for_high_ns_scores() | check_for_low_sp_scores()) {
	scoring_error();
	if (!force)
	    exit(EX_ERROR);
    }

    /*
    ** 5.  Calculate fp target
    ** The fp target will be derived thus: score non-spams with s and md as
    ** shipped, and determine the count that will result from a spam cutoff
    ** of 0.95; if that is < 0.25%, try 0.9375 etc.
    */
    
    min_dev = 0.02;

    target = get_thresh(ns_cnt, ns_scores);
    printf("False-positive target is %d (cutoff %8.6f)\n", target, spam_cutoff);

    /*
    ** 6.  Calculate x
    ** Calculate x with bogoutil's -r option (a new addition).
    ** Bound the calculated value within [0.4, 0.6] and set the range to be
    ** investigated to [x-0.1, x+0.1].
    */
    if (user_robx > 0.0)
	robx = user_robx;
    else
	robx = robx_init();

    robx = ROUND(robx*1000.0)/1000.0;

    printf("Initial x value is %5.3f\n", robx);

    /* No longer needed */
    wordhash_free(ns_and_sp->train);
    ns_and_sp->train = NULL;

    if (memdebug) { MEMDISPLAY; }

    if (ns_cnt >= TEST_COUNT && sp_cnt >= TEST_COUNT)	/* HACK */
    for (scan=0; scan <= 1; scan += 1) {
	bool f;
	uint i;
	uint r_count;
	uint rsi, rxi, mdi;
	result_t *results, *r;
	result_t *sorted;

	printf("Performing %s scan:\n", scan==0 ? "coarse" : "fine");

	switch (scan) {
	case 0:		/* COARSE */
	    /*
	    ** 7.  Coarsely scan s, md and x
	    ** The coarse s scan will range from 1 to 0.01 in half decades, and the
	    ** coarse md scan will range from 0.05 to 0.45 in steps of 0.05.  The
	    ** coarse x scan will use steps of 0.05. The trough must be surrounded on
	    ** six sides by values below the 33% quantile (unless bounded on one or
	    ** more sides).
	    */
	    init_coarse(robx);
	    break;
	case 1:		/* FINE */
	    /*
	    ** 8.  Finely scan the peak region
	    ** The fine s scan will range over the estimated s +/- half a decade in
	    ** steps of a quarter decade, and the fine md scan will range over the
	    ** estimated md +/- 0.075 in steps of 0.015.  The fine x scan will range
	    ** over the estimated x +/- 0.04 in steps of 0.02.  Scans of s and md
	    ** are bounded by the limits of the coarse scan.  Again, the trough must
	    ** be surrounded on six sides by values below the 33% quantile.  If no
	    ** such trough exists, a warning is given.
	    */
	    init_fine(robs, min_dev, robx);
	    break;
	}

	if (verbose >= PARMS)
	    print_all_parms();

	r_count = rsval->cnt * mdval->cnt * rxval->cnt;
	results = (result_t *) xcalloc(rsval->cnt * mdval->cnt * rxval->cnt, sizeof(result_t));

	cnt = 0;
	beg = time(NULL);
	for (rsi = 0; rsi < rsval->cnt; rsi += 1) {
	    robs = rsval->data[rsi];
	    for (mdi = 0; mdi < mdval->cnt; mdi += 1) {
		min_dev = mdval->data[mdi];
		for (rxi = 0; rxi < rxval->cnt; rxi += 1) {
		    uint fp, fn;
		    robx = rxval->data[rxi];

		    spam_cutoff = 0.01;
		    score_ns(ns_scores);	/* scores in ascending order */
		    qsort(ns_scores, ns_cnt, sizeof(double), compare_double);

		    /* Determine spam_cutoff and false_pos */
		    for (fp = target; fp < ns_cnt; fp += 1) {
			spam_cutoff = ns_scores[ns_cnt - fp];
			if (spam_cutoff < 0.999999)
			    break;
		    }
		    if (ns_cnt < fp)
			fprintf(stderr,
				"Too few false positives to determine a valid cutoff\n");
		    score_sp(sp_scores);
		    fn = get_fn_count(sp_cnt, sp_scores);

		    /* save parms and results */
		    r = &results[cnt++];
		    r->idx = cnt;
		    r->rsi = rsi; r->rs = robs;
		    r->rxi = rxi; r->rx = robx;
		    r->mdi = mdi; r->md = min_dev;
		    r->co = spam_cutoff;
		    r->fp = fp;
		    r->fn = fn;

		    if (verbose >= SUMMARY) {
			if (verbose > SUMMARY)
			    printf( "%3d  %d %d %d  ", cnt, rsi, mdi, rxi);
			printf( "%6.4f %5.3f %5.3f %8.6f %2d %3d\n", robs, min_dev, robx, spam_cutoff, fp, fn);
			fflush(stdout);
		    }
		    else {
			progress(cnt, r_count);
		    }
		}
	    }
	    fflush(stdout);
	}

	if (verbose >= SUMMARY) {
	    end = time(NULL);
	    show_elapsed_time(beg, end, cnt, (double)(end-beg)/cnt, "iterations", "secs");
	}

	printf("\n");

	/* Scan complete, now find minima */
	f = false;
	for (i = 0; i < cnt; i += 1) {
	    r = &results[i];
	    if (r->fp == target)
		f = true;
	}
	if (!f)
	    printf("Warning: fp target was not met, using original results\n");

	sorted = results_sort(r_count, results);
	top_ten(sorted);

	r = count_outliers(r_count, sorted, results);
	robs = rsval->data[r->rsi];
	robx = rxval->data[r->rxi];
	min_dev = mdval->data[r->mdi];

	printf("Minimum found at s %6.4f, md %5.3f, x %5.3f\n", robs, min_dev, robx);
	printf("        fp %d (%6.4f%%), fn %d (%6.4f%%)\n",
		r->fp, r->fp*100.0/ns_cnt, 
		r->fn, r->fn*100.0/sp_cnt);
	printf("\n");

	data_free(rsval);
	data_free(rxval);
	data_free(mdval);

	xfree(results);
	xfree(sorted);
    }

    /*
    ** 9.  Suggest possible spam and non-spam cutoff values
    ** With the final x, md and s values, score the spams and non-spams and
    ** sort the non-spam scores decreasing and the spam scores increasing;
    ** then, traverse the non-spam list until the 0.2% point; report cutoffs
    ** that give 0.05%, 0.1% and 0.2% fp.
    */

    if (ns_cnt >= TEST_COUNT && sp_cnt >= TEST_COUNT)	/* HACK */
    final_recommendations();

    if (memdebug) { MEMDISPLAY; }

    return status;
}

int main(int argc, char **argv) /*@globals errno,stderr,stdout@*/
{
    ex_t exitcode = EX_OK;

    atexit(bt_exit);

    fBogotune = true;		/* for rob_compute_spamicity() */
    no_stats = true;		/* suppress rstats */

    dbgout = stdout;
    debug_mask=MASK_BIT('R');
    debug_mask=0;

    progtype = build_progtype(progname, DB_TYPE);

    /* directories from command line and config file are already handled */

    ds_file = get_directory(PR_ENV_BOGO);
    if (ds_file == NULL)
	ds_file = get_directory(PR_ENV_HOME);

    ham_files  = filelist_new("ham");
    spam_files = filelist_new("spam");

    /* process args and read mailboxes */
    process_args(argc, argv);

    bogotune();

    bogotune_free();

    MEMDISPLAY;

    exit(exitcode);
}

static void bt_exit(void)
{
    return;
}

static void show_elapsed_time(int beg, int end, uint cnt, double val, 
			      const char *lbl1, const char *lbl2)
{
    int tm = end - beg;
    printf("    %2dm:%02ds for %d %s.  avg: %5.1f %s\n",
	   MIN(tm), SECONDS(tm), cnt, lbl1, val, lbl2);
}

/* End */
