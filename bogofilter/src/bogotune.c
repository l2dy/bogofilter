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
#include "robx.h"
#include "rstats.h"
#include "token.h"
#include "tunelist.h"
#include "wordhash.h"
#include "wordlists.h"
#include "xmalloc.h"
#include "xstrdup.h"

#define	MSG_COUNT	".MSG_COUNT"

#define	TEST_COUNT	500	/* minimum allowable message count */
#define	PREF_COUNT	4000	/* preferred message count         */
#define	LIST_COUNT	2000	/* minimum msg count in tunelist   */

#define	HAM_CUTOFF	0.10
#define	MIN_CUTOFF	0.55	/* minimum for get_thresh() */
#define	MAX_CUTOFF	0.99	/* maximum for get_thresh() */
#define	SPAM_CUTOFF	0.95
#define FP_CUTOFF	0.999

/* bogotune's default parameters */
#define	DEFAULT_ROBS	0.01
#define	DEFAULT_ROBX	0.415
#define	DEFAULT_MIN_DEV	0.02

#define	MD_MIN_C	0.05	/* smallest min_dev to test */
#define	MD_MAX_C	0.45	/* largest  min_dev to test */
#define	MD_DLT_C	0.05	/* increment		    */

#define	MD_MIN_F	0.02
#define	MD_MAX_F	MD_MAX_C+MD_DLT_F
#define	MD_DLT_F	0.015

enum e_verbosity {
    SUMMARY	   = 1,	/* summarize main loop iterations */
    PARMS	   = 2,	/* print parameter sets (rs, md, rx) */
    SCORE_SUMMARY  = 4,	/* verbosity level for printing scores */
    SCORE_DETAIL	/* verbosity level for printing scores */
};

#define	MOD(n,m)	((n) - ((int)((n)/(m)))*(m))

#define	MIN(n)		((n)/60)
#define SECONDS(n)	((n) - MIN(n)*60)

/* Global Variables */

const char *progname = "bogotune";
char *ds_file;

extern double robx, robs;

word_t *w_msg_count;
wordhash_t *t_ns, *t_sp;

tunelist_t *ns_and_sp;
tunelist_t *ns_msglists, *sp_msglists;

flhead_t *spam_files, *ham_files;

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
static int  load_hook(word_t *key, dsv_t *data, void *userdata);
static void load_wordlist(ds_foreach_t *hook, void *userdata);
static void show_elapsed_time(int beg, int end, uint cnt, double val, 
			      const char *lbl1, const char *lbl2);

/* Function Definitions */

static void bt_trap(void) {} 

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

static data_t *seq_canonical(double fst, double amt)
{
    uint i = 0;
    data_t *val = xcalloc(1, sizeof(data_t));

    val->cnt = 5;
    val->data = xcalloc(val->cnt, sizeof(double));

    val->data[i++] = fst;
    val->data[i++] = fst - amt;
    val->data[i++] = fst + amt;
    val->data[i++] = fst - amt * 2;
    val->data[i++] = fst + amt * 2;

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
    rxval = seq_canonical(_rx, 0.05);
    rsval = seq_by_pow(0.0, -2.0, -0.5);
    mdval = seq_by_amt(MD_MIN_C, MD_MAX_C, MD_DLT_C);
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
    if (s0 < MD_MIN_F) s0 = MD_MIN_F;
    if (s1 > MD_MAX_F) s1 = MD_MAX_F;

    mdval = seq_by_amt(s0, s1, MD_DLT_F);
    rxval = seq_canonical(_rx, 0.02);
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

static int compare_ascending(const void *const ir1, const void *const ir2)
{
    const double d1 = *(const double *)ir1;
    const double d2 = *(const double *)ir2;

    if (d1 - d2 > 0.0) return  1;
    if (d2 - d1 > 0.0) return -1;

    return 0;
}

static int compare_descending(const void *const ir1, const void *const ir2)
{
    const double d1 = *(const double *)ir1;
    const double d2 = *(const double *)ir2;

    if (d1 - d2 > 0.0) return -1;
    if (d2 - d1 > 0.0) return +1;

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

    qsort(results, count, sizeof(double), compare_descending);

    return;
}

static bool check_for_high_ns_scores(void)
{
    double percent = 0.0025;
    uint target = ceil(ns_cnt * percent);

    score_ns(ns_scores);	/* scores in descending order */

    if (ns_scores[target-1] < SPAM_CUTOFF)
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

    qsort(results, count, sizeof(double), compare_ascending);

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
    uint target = ceil(sp_cnt * percent);

    score_sp(sp_scores);			/* get scores */

    if (sp_scores[target-1] > HAM_CUTOFF)
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
    for (i = 0; i < 10 && ns_scores[i] > SPAM_CUTOFF; i += 1) 
	printf("      %2d %8.6f\n", i+1, ns_scores[i]);

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
    double cutoff, percent;

    score_ns(scores);				/* get scores */

    cutoff = 0.0;
    for (percent = scale(count, TEST_COUNT, PREF_COUNT, 0.0050, 0.0025);
	 percent > 0.0001;
	 percent -= 0.0001) {
	ftarget = ceil(count * percent);	/* compute fp count */
	if (ftarget <= mtarget)
	    break;
	cutoff = scores[ftarget-1];		/* get cutoff value */
	if (cutoff >= MIN_CUTOFF)
	    break;
    }

    if (verbose >= PARMS)
	printf("mtarget %d, ftarget %d, percent %6.4f%%\n", mtarget, ftarget, percent * 100.0);

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
	cutoff = scores[ftarget-1];

    return ftarget;
}

static uint read_mailbox(char *arg)
{
    uint count = 0;
    wordhash_t *train = ns_and_sp->train;
    mlhead_t *msgs = (run_type == REG_GOOD) ? ns_msglists->msgs : sp_msglists->msgs;

    if (verbose) {
	printf("Reading %s\n", arg);
	fflush(stdout);
    }

    mbox_mode = true;
    bogoreader_init(1, &arg);
    while ((*reader_more)()) {
	wordhash_t *whc, *whp = NULL;
	wordprop_t *msg_count;

	whc = wordhash_new();

	collect_words(whc);
	
	if (msgs_good == 0 && msgs_bad == 0) {
	    msg_count = wordhash_insert(whc, w_msg_count, sizeof(wordprop_t), NULL);
	    if (msg_count->cnts.good == 0 ||msg_count->cnts.bad == 0)
		load_wordlist(load_hook, ns_and_sp->train);
	    if (msgs_good == 0 && msgs_bad == 0)
		fprintf(stderr,
			"Can't find .MSG_COUNT.\n");
	    exit(EX_ERROR);
	}

	if (whc->count == 0) {
	    printf("msg #%d, count is %d\n", count, whc->count);
	    bt_trap();
	}

	if (whc->count != 0) {
	    count += 1;
	    if (!msg_count_file)
		whp = convert_wordhash_to_propslist(whc, train);
	    else
		whp = convert_propslist_to_countlist(whc);
	    msglist_add(msgs, whp);
	}

	if (whc != whp)
	    wordhash_free(whc);

	if (verbose && (count % 100) == 0) {
	    if ((count % 1000) != 0)
		putchar('.');
	    else
		printf("\r              \r%d ", count/1000 );
	    fflush(stdout);
	}
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

static void distribute(int mode, tunelist_t *ns_or_sp)
{
    int good = mode == REG_GOOD;
    int bad  = 1 - good;

    bool divvy = ds_file == NULL && user_robx < EPS;

    wordhash_t *train = ns_and_sp->train;
    mlhead_t *msgs = ns_or_sp->msgs;
    mlitem_t *item;
    wordprop_t *msg_count;

    int score_count = 0;
    int train_count = 0;

    double ratio = (ds_file != NULL || user_robx < EPS)
	? 0.0
	: scale(msgs->count,  
		LIST_COUNT + TEST_COUNT,	/* small count */
		LIST_COUNT + LIST_COUNT,	/* large count */
		LIST_COUNT / TEST_COUNT,	/* small ratio */
		LIST_COUNT / LIST_COUNT);	/* large ratio */
    
    /* Update .MSG_COUNT */
    msg_count = wordhash_insert(train, w_msg_count, sizeof(wordprop_t), &wordprop_init);

    for (item = msgs->head; item != NULL; item = item->next) {
	wordhash_t *wh = item->wh;

	/* training set */
	if (divvy && train_count / ratio < score_count + 1) {
	    wordhash_set_counts(wh, good, bad);
	    wordhash_add(train, wh, &wordprop_init);
	    train_count += 1;
	    msg_count->cnts.good += good;
	    msg_count->cnts.bad  += bad;
	    wordhash_free(wh);
	}
	/* scoring set  */
	else {
	    uint bin = divvy ? MOD(score_count,3) : 0;
	    msglist_add(ns_or_sp->u.sets[bin], wh);
	    score_count += 1;
	}
	item->wh = NULL;
    }

    return;
}

static void create_countlists(tunelist_t *ns_or_sp)
{
    uint i;
    uint c = COUNTOF(ns_or_sp->u.sets);
    for (i=0; i<c; i += 1) {
	mlhead_t *list = ns_or_sp->u.sets[i];
	mlitem_t *item;

	for (item = list->head; item != NULL; item = item->next) {
	    wordhash_t *who = item->wh;
	    wordhash_t *whn = convert_propslist_to_countlist(who);
	    if (whn != who) {
		wordhash_free(who);
		item->wh = whn;
	    }
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

static void load_wordlist(ds_foreach_t *hook, void *userdata)
{
    struct stat sb;
    size_t len = strlen(ds_file) + strlen(WORDLIST) + 2;
    char *path = xmalloc(len);

    if (stat(ds_file, &sb) != 0) {
	fprintf(stderr, "Error accessing file or directory '%s'.\n", ds_file);
	if (errno != 0)
	    fprintf(stderr, "error #%d - %s.\n", errno, strerror(errno));
	return;
    }

    build_path(path, len, ds_file, WORDLIST);

    if (verbose) {
	printf("Reading %s\n", path);
	fflush(stdout);
    }

    ds_oper(path, DB_READ, hook, userdata);
    db_cachesize = ceil(sb.st_size / 3.0 / 1024.0 / 1024.0);

    xfree(path);

    return;
}

static int load_hook(word_t *key, dsv_t *data, void *userdata)
/* returns 0 if ok, 1 if not ok */
{
    wordhash_t *train = userdata;

    wordprop_t *tokenprop = wordhash_insert(train, key, sizeof(wordprop_t), &wordprop_init);
    tokenprop->cnts.bad = data->spamcount;
    tokenprop->cnts.good = data->goodcount;

    return 0;
}

static double get_robx(void)
{
    double rx;
    if (user_robx > 0.0)
	return user_robx;

    printf("Calculating initial x value...\n");

    rx = compute_robinson_x(ds_file);

    if (rx > 0.6) rx = 0.6;
    if (rx < 0.4) rx = 0.4;

    printf("Initial x value is %8.6f\n", rx);

    return rx;
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
    printf("   rs     md    rx    co      %%fp     %%fn    fp  fn\n" );

    for (i=0; i < 10; i += 1) {
	result_t *r = &sorted[i];
	if (verbose)
	    printf("%3d  ", r->idx);
	printf( "%6.4f %5.3f %5.3f %6.4f  %6.4f  %6.4f  %3d %3d\n",
		r->rs, r->md, r->rx, r->co, 
		r->fp*100.0/ns_cnt, r->fn*100.0/sp_cnt, r->fp, r->fn);
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
	printf("   %2d, %2d, %2d, %2d\n", rsi, mdi, rxi, fn);
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
     printf("\r%3d [", cur);
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
    uint minn[] = { 10000, 2000, 1000, 500, 1 };

    printf("Performing final scoring:\n");

    printf("Non-Spam...\n");
    score_ns(ns_scores);		/* get scores (in descending order) */

    printf("Spam...\n");
    score_sp(sp_scores);		/* get scores (in ascending order) */

    if (verbose >= PARMS)
	printf("# ns_cnt %d, sp_cnt %d\n", ns_cnt, sp_cnt);

    printf("Recommendations:\n\n");
    printf("---cut---\n");
    printf("db_cachesize=%d\n", db_cachesize);

    printf("robx=%8.6f\n", robx);
    printf("min_dev=%5.3f\n", min_dev);
    printf("robs=%6.4f\n", robs);

    for (m=0; m < COUNTOF(minn); m += 1) {
	double cutoff;
	uint i, fp = 0, fn = 0;
	uint mn = minn[m];
	double fpp, fnp;

	if (ns_cnt < mn)
	    continue;

	if (mn > 1 ) {
	    uint t = (ns_cnt + mn - 1) / mn;
	    cutoff = ns_scores[t-1];
	    if (cutoff > FP_CUTOFF)
		continue;
	    fpp = 100.0 / mn;
	    fp = ns_cnt / mn;
	}
	else {
	    cutoff = SPAM_CUTOFF;
	    if (printed)
		break;
	    for (i = 0; i < ns_cnt && ns_scores[i-1] < cutoff; i += 1)
		fp = i;
	    if (fp == 0) {
		fprintf(stderr, 
			"Too few low scoring spam.\n");
		exit(EX_ERROR);
	    }
	    fp -= 1;
	    cutoff = ns_scores[fp-1];
	    fpp = 100.0 * (ns_cnt - fp) / ns_cnt;
	}

	for (i = 0; i < sp_cnt; i += 1) {
	    if (sp_scores[i] >= cutoff) {
		fn = i;
		break;
	    }
	}
	fnp = 100.0 * fn / sp_cnt;

	if (printed)  printf("#");
	printf("spam_cutoff=%5.3f\t# for %4.2f%% false pos (%d); expect %4.2f%% false neg (%d).\n",
	       cutoff, fpp, fp, fnp, fn);

	printed = true;
    }

    s = ceil(sp_cnt * 0.002 - 1);
    ham_cutoff = sp_scores[s];
    if (ham_cutoff < 0.10) ham_cutoff = 0.10;
    if (ham_cutoff > 0.45) ham_cutoff = 0.45;
    printf("ham_cutoff=%5.3f\t\n", ham_cutoff);
    printf("---cut---\n");
    printf("\n");
    printf("Tuning completed.\n");
}

static void bogotune_init(void)
{
    const char *msg_count = MSG_COUNT;
    w_msg_count = word_new((const byte *)msg_count, strlen(msg_count));
    ns_and_sp   = tunelist_new("tr");		/* training lists */
    ns_msglists = tunelist_new("ns");		/* non-spam scoring lists */
    sp_msglists = tunelist_new("sp");		/* spam     scoring lists */

    return;
}

static void bogotune_free(void)
{
    xfree(ns_scores);
    xfree(sp_scores);

    filelist_free(ham_files);
    filelist_free(spam_files);

    tunelist_free(ns_msglists);
    tunelist_free(sp_msglists);
    tunelist_free(ns_and_sp);

    word_free(w_msg_count);

    token_cleanup();
    mime_cleanup();

    return;
}

static bool check_msg_counts(void)
{
    bool ok = true;

    if (msgs_good < LIST_COUNT || msgs_bad < LIST_COUNT) {
	fprintf(stderr, 
		"The wordlist contains %u non-spam and %u spam messages.\n"
		"Bogotune must be run with at least %d of each.\n",
		msgs_good, msgs_bad, LIST_COUNT);
	ok = false;
    }

    if (msgs_bad < msgs_good / 5 ||
	msgs_bad > msgs_good * 5) {
	fprintf(stderr,
		"The wordlist has a ratio of spam to non-spam of %0.1f to 1.0.\n"
		"Bogotune requires the ratio be in the range of 0.2 to 5.\n",
		msgs_bad * 1.0 / msgs_good);
	ok = false;
    }

    return ok;
}

static rc_t bogotune(void)
{
    result_t *best;

    int beg, end;
    uint cnt, scan;
    uint target;
    rc_t status = RC_OK;

    bogotune_init();

    beg = time(NULL);

    ham_cutoff = 0.0;
    spam_cutoff = 0.1;

    /* Note: memory usage highest while reading messages */
    /* usage decreases as distribute() converts to count format */

    /* read all messages, merge training sets, look up scoring sets */
    ns_cnt = filelist_read(REG_GOOD, ham_files);
    sp_cnt = filelist_read(REG_SPAM, spam_files);

    end = time(NULL);
    if (verbose) {
	cnt = ns_cnt + sp_cnt;
	show_elapsed_time(beg, end, ns_cnt + sp_cnt, (double)cnt/(end-beg), "messages", "msg/sec");
    }

    distribute(REG_SPAM, sp_msglists);
    distribute(REG_GOOD, ns_msglists);

    create_countlists(ns_msglists);
    create_countlists(sp_msglists);

    if (verbose && time(NULL) - end > 2) {
	end = time(NULL);
	cnt = ns_cnt + sp_cnt;
	show_elapsed_time(beg, end, ns_cnt + sp_cnt, (double)cnt/(end-beg), "messages", "msg/sec");
    }

    if (verbose > 3) {
	tunelist_print(ns_and_sp);
	tunelist_print(ns_msglists);
	tunelist_print(sp_msglists);
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

    robs = DEFAULT_ROBS;
    robx = DEFAULT_ROBX;
    min_dev = DEFAULT_MIN_DEV;

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
    spam_cutoff = ns_scores[target-1];
    printf("False-positive target is %d (cutoff %8.6f)\n", target, spam_cutoff);

    /*
    ** 6.  Calculate x
    ** Calculate x with bogoutil's -r option (a new addition).
    ** Bound the calculated value within [0.4, 0.6] and set the range to be
    ** investigated to [x-0.1, x+0.1].
    */

    if (user_robx > EPS)
	robx = user_robx;
    else if (ds_file != NULL)
	robx = get_robx();

    /* No longer needed */
    wordhash_free(ns_and_sp->train);
    ns_and_sp->train = NULL;

    if (!check_msg_counts())
	exit(EX_ERROR);

    for (scan=0; scan <= 1; scan += 1) {
	bool f;
	uint i;
	uint r_count;
	uint rsi, rxi, mdi;
	result_t *results, *r, *sorted;

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

		    /* save parms */
		    r = &results[cnt++];
		    r->idx = cnt;
		    r->rsi = rsi; r->rs = robs;
		    r->rxi = rxi; r->rx = robx;
		    r->mdi = mdi; r->md = min_dev;

		    if (verbose > SUMMARY)
			printf( "%3d  %d %d %d  ", cnt, rsi, mdi, rxi);
		    if (verbose >= SUMMARY) {
			printf( "%6.4f %5.3f %5.3f", robs, min_dev, robx);
			fflush(stdout);
		    }

		    spam_cutoff = 0.01;
		    score_ns(ns_scores);	/* scores in descending order */
		    
		    /* Determine spam_cutoff and false_pos */
		    for (fp = target; fp < ns_cnt; fp += 1) {
			spam_cutoff = ns_scores[fp-1];
			if (spam_cutoff < 0.999999)
			    break;
		    }
		    if (ns_cnt < fp)
			fprintf(stderr,
				"Too few false positives to determine a valid cutoff\n");

		    score_sp(sp_scores);	/* scores in ascending order */
		    fn = get_fn_count(sp_cnt, sp_scores);

		    /* save results */
		    r->co = spam_cutoff;
		    r->fp = fp;
		    r->fn = fn;

		    if (verbose < SUMMARY)
			progress(cnt, r_count);
		    else {
			printf(" %8.6f %2d %3d\n", spam_cutoff, fp, fn);
			fflush(stdout);
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
	    best = &results[i];
	    if (best->fp == target)
		f = true;
	}
	if (!f)
	    printf("Warning: fp target was not met, using original results\n");

	sorted = results_sort(r_count, results);
	top_ten(sorted);

	best = count_outliers(r_count, sorted, results);
	robs = rsval->data[best->rsi];
	robx = rxval->data[best->rxi];
	min_dev = mdval->data[best->mdi];

	printf("Minimum found at s %6.4f, md %5.3f, x %5.3f\n", robs, min_dev, robx);
	printf("        fp %d (%6.4f%%), fn %d (%6.4f%%)\n",
		best->fp, best->fp*100.0/ns_cnt, 
		best->fn, best->fn*100.0/sp_cnt);
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

    robs = rsval->data[best->rsi];
    robx = rxval->data[best->rxi];
    min_dev = mdval->data[best->mdi];

    final_recommendations();

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
