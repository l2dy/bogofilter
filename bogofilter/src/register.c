/* $Id$ */

/* register.c -- read input with collect and register to persistent db */

#include "common.h"

#include <stdlib.h>

#include "bogofilter.h"
#include "datastore.h"
#include "collect.h"
#include "format.h"
#include "maint.h"			/* for today */
#include "register.h"
#include "wordhash.h"
#include "wordlists.h"

#define PLURAL(count) ((count == 1) ? "" : "s")

extern char msg_register[];
extern size_t msg_register_size;

/*
 * tokenize text on stdin and register it to a specified list
 * and possibly out of another list
 */
void register_words(run_t _run_type, wordhash_t *h, int msgcount)
{
  const char *r="",*u="";
  hashnode_t *node;
  wordprop_t *wordprop;
  run_t save_run_type = run_type;

  int wordcount = h->count;	/* use number of unique tokens */

  dsv_t val;

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
    (void)fprintf(dbgout, "# %d word%s, %d message%s\n", 
		  wordcount, PLURAL(wordcount), msgcount, PLURAL(msgcount));

  /* When using auto-update with separate wordlists , 
     datastore.c needs to know which to update */

  run_type |= _run_type;

/*
  set_list_active_status(false);
*/

  for (node = wordhash_first(h); node != NULL; node = wordhash_next(h))
  {
      wordprop = node->buf;
      ds_read(word_list->dsh, node->key, &val);
      if (incr != IX_UNDF) {
	  uint32_t *counts = val.count;
	  counts[incr] += wordprop->freq;
      }
      if (decr != IX_UNDF) {
	  uint32_t *counts = val.count;
	  counts[decr] = ((long)counts[decr] < wordprop->freq) ? 0 : counts[decr] - wordprop->freq;
      }
      ds_write(word_list->dsh, node->key, &val);
  }

  for (list = word_lists; list != NULL; list = list->next)
  {
/*
      if (!list->active)
	  continue;
*/

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

      ds_flush(list->dsh);

      if (verbose>1)
	  (void)fprintf(stderr, "bogofilter: list %s - %ld spam, %ld good\n",
			list->filename, list->msgcount[IX_SPAM], list->msgcount[IX_GOOD]);
  }

  run_type = save_run_type;
}

/* this function accumulates the word frequencies from the src hash to
 * those of the dest hash
 */
void add_hash(wordhash_t *dest, wordhash_t *src) {
    wordprop_t *d;
    hashnode_t *s;

    int count = dest->count + src->count;	/* use dest count as total */

    dest->wordcount += src->wordcount;

    for (s = wordhash_first(src); s; s = wordhash_next(src)) {
	d = wordhash_insert(dest, s->key, sizeof(wordprop_t), &wordprop_init);
	d -> freq += ((wordprop_t *)(s -> buf)) ->freq;
    }

    dest->count = count;
}
