/* $Id$ */

/* register.c -- read input with collect and register to persistent db */

#include "common.h"

#include <stdlib.h>

#include "bogofilter.h"
#include "datastore.h"
#include "collect.h"
#include "format.h"
#include "msgcounts.h"
#include "rand_sleep.h"
#include "register.h"
#include "wordhash.h"
#include "wordlists.h"

#define PLURAL(count) ((count == 1) ? "" : "s")

/**
 * register wordhash to a specified list
 * and possibly out of another list
 * exits the program in case of error.
 */
void register_words(run_t _run_type, wordhash_t *h, u_int32_t msgcount)
{
    const char *r="",*u="";
    hashnode_t *node;
    wordprop_t *wordprop;
    dsv_t val;
    int retrycount = 5;			/* we'll retry an aborted
					   registration five times
					   before giving up. */

    u_int32_t g = 0, b = 0;
    u_int32_t wordcount = h->count;	/* use number of unique tokens */

    sh_t incr = IX_UNDF, decr = IX_UNDF;

    /* If update directory explicity supplied, setup the wordlists. */
    if (update_dir) {
	if (setup_wordlists(update_dir, PR_CFG_UPDATE) != 0)
	    exit(EX_ERROR);
    }

    if (_run_type & REG_SPAM)	{ r = "s"; incr = IX_SPAM; }
    if (_run_type & REG_GOOD)	{ r = "n"; incr = IX_GOOD; }
    if (_run_type & UNREG_SPAM)	{ u = "S"; decr = IX_SPAM; }
    if (_run_type & UNREG_GOOD)	{ u = "N"; decr = IX_GOOD; }

    if (wordcount == 0)
	msgcount = 0;

    format_log_update(msg_register, msg_register_size, u, r,
	    wordcount, msgcount);

    if (verbose)
	(void)fprintf(dbgout, "# %u word%s, %u message%s\n",
		      wordcount, PLURAL(wordcount), msgcount, PLURAL(msgcount));

retry:
    if (ds_txn_begin(word_list->dsh)) {
	fprintf(stderr, "ds_txn_begin error.\n");
	exit(EX_ERROR);
    }

    /* register words */
    for (node = wordhash_first(h); node != NULL; node = wordhash_next(h))
    {
	wordprop = node->buf;
	ds_read(word_list->dsh, node->key, &val);
	if (incr != IX_UNDF) {
	    u_int32_t *counts = val.count;
	    counts[incr] += wordprop->freq;
	}
	if (decr != IX_UNDF) {
	    u_int32_t *counts = val.count;
	    counts[decr] = ((long)counts[decr] < wordprop->freq) ? 0 : counts[decr] - wordprop->freq;
	}
	ds_write(word_list->dsh, node->key, &val);
    }

    /* register counts */
    ds_get_msgcounts(word_list->dsh, &val);
    word_list->msgcount[IX_SPAM] = val.spamcount;
    word_list->msgcount[IX_GOOD] = val.goodcount;

    if (incr != IX_UNDF)
	word_list->msgcount[incr] += msgcount;

    if (decr != IX_UNDF) {
	if (word_list->msgcount[decr] > msgcount)
	    word_list->msgcount[decr] -= msgcount;
	else
	    word_list->msgcount[decr] = 0;
    }

    val.spamcount = word_list->msgcount[IX_SPAM];
    val.goodcount = word_list->msgcount[IX_GOOD];

    ds_set_msgcounts(word_list->dsh, &val);

    switch(ds_txn_commit(word_list->dsh)) {
	case DST_OK:
	    break;
	case DST_TEMPFAIL:
	    if (--retrycount) {
		fprintf(stderr, "commit was aborted, retrying...\n");
		rand_sleep(4 * 1000, 1000 * 1000);
		goto retry;
	    }
	    fprintf(stderr, "giving up on this transaction.\n");
	    exit(EX_ERROR);
	case DST_FAILURE:
	    fprintf(stderr, "commit failed.\n");
	    exit(EX_ERROR);
	default:
	    fprintf(stderr, "unknown return.\n");
	    abort();
    }

    ds_flush(word_list->dsh);

    if (DEBUG_REGISTER(1))
	(void)fprintf(dbgout, "bogofilter: word_list %s - %ul spam, %ul good\n",
		      word_list->filename, word_list->msgcount[IX_SPAM], word_list->msgcount[IX_GOOD]);

    g += val.goodcount;
    b += val.spamcount;

    set_msg_counts(g, b);
}
