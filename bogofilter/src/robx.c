/* $Id$ */

/*****************************************************************************

NAME:
   robx.c -- computes robx value by reading wordlist.db

AUTHOR:
   David Relson - C version
   Greg Lous - perl version
   
******************************************************************************/

#include "common.h"

#include "datastore.h"
#include "robx.h"
#include "wordlists.h"

/* Function Prototypes */

/* Function Definitions */

typedef struct robhook_data {
    double   sum;
    uint32_t count;
    dsh_t    *dsh;
    double   scalefactor;
} rhd_t;

static void robx_accum(rhd_t *rh, 
		       word_t *key,
		       dsv_t *data)
{
    uint32_t goodness = data->goodcount;
    uint32_t spamness = data->spamcount;
    double prob = spamness / (goodness * rh->scalefactor + spamness);
    bool doit = goodness + spamness >= 10;

    if (doit) {
	rh->sum += prob;
	rh->count += 1;
    }

    /* print if -vvv and token in both word lists, or -vvvv */
    if ((verbose > 2 && doit) || verbose > 3) {
	fprintf(dbgout, "cnt: %4lu,  sum: %11.6f,  ratio: %9.6f,"
		"  sp: %3lu,  gd: %3lu,  p: %9.6f,  t: %*s\n", 
		(unsigned long)rh->count, rh->sum, rh->sum / rh->count,
		(unsigned long)spamness, (unsigned long)goodness, prob,
		(int)min(INT_MAX,key->leng), key->text);
    }
}

static int robx_hook(word_t *key, dsv_t *data, 
		     void *userdata)
{
    struct robhook_data *rh = userdata;
    dsh_t *dsh = rh->dsh;
    sh_t i = dsh->index;

    bool doit;

    /* ignore system meta-data */
    if (*key->text == '.')
	return 0;

    if (dsh->count == 1) {
	doit = true;
    } else {
	/* tokens in good list were already counted */
	/* now add in tokens only in spam list */
	ds_read(dsh, key, data);
	doit = data->goodcount == 0;
    }

    if (doit)
	robx_accum(rh, key, data);

    dsh->index = i;

    return 0;
}

static int count_hook(word_t *key, dsv_t *data, 
		      void *userdata)
{
    struct robhook_data *rh = userdata;
    dsh_t *dsh = rh->dsh;
    sh_t i = dsh->index;

    /* ignore system meta-data */
    if (*key->text == '.')
	return 0;

    ds_read(dsh, key, data);

    /* skip tokens with goodness == 0 */
    if (data->goodcount != 0)
	robx_accum(rh, key, data);

    dsh->index = i;

    return 0;
}

static double compute_robx(dsh_t *dsh)
{
    double rx;

    dsv_t val;
    bool ok;
    uint32_t good_cnt, spam_cnt;
    struct robhook_data rh;

    ok = ds_get_msgcounts(dsh, &val);

    if (!ok) {
	fprintf(stderr, "Can't find message counts.\n");
	exit(EX_ERROR);
    }

    spam_cnt = val.spamcount;
    good_cnt = val.goodcount;

    rh.scalefactor = (double)spam_cnt/(double)good_cnt;
    rh.dsh = dsh;
    rh.sum = 0.0;
    rh.count = 0;

    if (dsh->count == 1) {
	dsh->index = 0;
	ds_foreach(dsh, robx_hook, &rh);
    }
    else {
	dsh->index = IX_GOOD;	    /* robx needs count of good tokens */
	ds_foreach(dsh, count_hook, &rh);

	dsh->index = IX_SPAM;	    /* and scores for spam spam tokens */
	ds_foreach(dsh, robx_hook, &rh);
    }

    rx = rh.sum/rh.count;
    if (verbose > 2)
	printf("%s: %lu, %lu, scale: %f, sum: %f, cnt: %6d, .ROBX: %f\n",
	       MSG_COUNT,
	       (unsigned long)spam_cnt, (unsigned long)good_cnt,
	       rh.scalefactor, rh.sum, (int)rh.count, rx);

    return rx;
}

double compute_robinson_x(char *path)
{
    double rx;

    setup_wordlists(path, PR_NONE);
    open_wordlists(DB_READ);

    rx = compute_robx(word_lists->dsh);

    close_wordlists(false);
    free_wordlists();

    return rx;
}
