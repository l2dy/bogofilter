/* $Id$ */

/* register.c -- read input with collect and register to persistent db */

#include "common.h"

#include <stdlib.h>

#include "bogofilter.h"
#include "datastore.h"
#include "collect.h"
#include "format.h"
#include "msgcounts.h"
#include "register.h"
#include "wordhash.h"
#include "wordlists.h"

#define PLURAL(count) ((count == 1) ? "" : "s")

/*
 * tokenize text on stdin and register it to a specified list
 * and possibly out of another list
 */
void register_words(run_t _run_type, wordhash_t *h, u_int32_t msgcount)
{
  const char *r="",*u="";
  hashnode_t *node;
  wordprop_t *wordprop;
  run_t save_run_type = run_type;

  u_int32_t g = 0, b = 0;
  u_int32_t wordcount = h->count;	/* use number of unique tokens */

  wordlist_t *list;
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

  format_log_update(msg_register, msg_register_size, u, r, wordcount, msgcount);

  if (verbose)
    (void)fprintf(dbgout, "# %u word%s, %u message%s\n", 
		  wordcount, PLURAL(wordcount), msgcount, PLURAL(msgcount));

  /* When using auto-update with separate wordlists , 
     datastore.c needs to know which to update */

  run_type |= _run_type;

retry:
  if (ds_txn_begin(word_list->dsh)) {
      fprintf(stderr, "ds_txn_begin error.\n");
      abort();
  }
  for (node = wordhash_first(h); node != NULL; node = wordhash_next(h))
  {
      dsv_t val;
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

  for (list = word_lists; list != NULL; list = list->next)
  {
      dsv_t val;

      ds_get_msgcounts(list->dsh, &val);
      list->msgcount[IX_SPAM] = val.spamcount;
      list->msgcount[IX_GOOD] = val.goodcount;

      if (incr != IX_UNDF)
	  list->msgcount[incr] += msgcount;
      
      if (decr != IX_UNDF) {
	  if (list->msgcount[decr] > msgcount)
	      list->msgcount[decr] -= msgcount;
	  else
	      list->msgcount[decr] = 0;
      }

      val.spamcount = list->msgcount[IX_SPAM];
      val.goodcount = list->msgcount[IX_GOOD];

      ds_set_msgcounts(list->dsh, &val);

      g += val.goodcount;
      b += val.spamcount;

      switch(ds_txn_commit(list->dsh)) {
	  case DST_OK:
	      break;
	  case DST_TEMPFAIL:
	      goto retry;
	  case DST_FAILURE:
	      fprintf(stderr, "commit failed.\n");
	      abort();
	  default:
	      fprintf(stderr, "unknown return.\n");
	      abort();
      }

      ds_flush(list->dsh);

      if (DEBUG_REGISTER(1))
	  (void)fprintf(dbgout, "bogofilter: list %s - %ul spam, %ul good\n",
			list->filename, list->msgcount[IX_SPAM], list->msgcount[IX_GOOD]);
  }

  set_msg_counts(g, b);

  run_type = save_run_type;
}

