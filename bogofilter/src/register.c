/* $Id$ */

/* register.c -- read input with collect and register to persistent db */

#include <stdio.h>
#include <stdlib.h>

#include <config.h>
#include "common.h"

#include "bogofilter.h"
#include "datastore.h"
#include "collect.h"
#include "format.h"
#include "register.h"
#include "wordhash.h"

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

  int wordcount = h->count;	/* use number of unique tokens */

  wordlist_t *list;
  int incr = -1, decr = -1;

  /* If update directory explicity supplied, setup the wordlists. */
  if (update_dir) {
      if (setup_wordlists(update_dir, PR_CFG_UPDATE) != 0)
	  exit(2);
  }

  if (_run_type & REG_SPAM)	{ r = "s"; incr = SPAM; }
  if (_run_type & REG_GOOD)	{ r = "n"; incr = GOOD; }
  if (_run_type & UNREG_SPAM)	{ u = "S"; decr = SPAM; }
  if (_run_type & UNREG_GOOD)	{ u = "N"; decr = GOOD; }

  if (wordcount == 0)
      msgcount = 0;

  format_log_update(msg_register, msg_register_size, u, r, wordcount, msgcount);

  if (verbose)
    (void)fprintf(dbgout, "# %d word%s, %d message%s\n", 
		  wordcount, PLURAL(wordcount), msgcount, PLURAL(msgcount));

/*
  set_list_active_status(false);
*/

  for (node = wordhash_first(h); node != NULL; node = wordhash_next(h)){
      wordprop = node->buf;
      if (incr >= 0) {
	  dbv_t val;
	  val.goodcount = val.spamcount = val.date = 0;
	  val.count[incr] = wordprop->freq;
	  db_increment(word_list->dbh, node->key, &val);
      }
      if (decr >= 0) {
	  dbv_t val;
	  val.goodcount = val.spamcount = val.date = 0;
	  val.count[decr] = wordprop->freq;
	  db_decrement(word_list->dbh, node->key, &val);
      }
  }

  for (list = word_lists; list != NULL; list = list->next){
      dbv_t val;

/*
      if (!list->active)
	  continue;
*/

      db_get_msgcounts(list->dbh, &val);
      list->msgcount[SPAM] = val.spamcount;
      list->msgcount[GOOD] = val.goodcount;

      if (incr >= 0)
	  list->msgcount[incr] += msgcount;
      
      if (decr >= 0) {
	  if (list->msgcount[decr] > msgcount)
	      list->msgcount[decr] -= msgcount;
	  else
	      list->msgcount[decr] = 0;
      }

      val.spamcount = list->msgcount[SPAM];
      val.goodcount = list->msgcount[GOOD];

      db_set_msgcounts(list->dbh, &val);

      db_flush(list->dbh);
      if (verbose>1)
	  (void)fprintf(stderr, "bogofilter: list %s - %ld spam, %ld good\n",
			list->filename, list->msgcount[SPAM], list->msgcount[GOOD]);
  }
}

/* this function accumulates the word frequencies from the src hash to
 * those of the dest hash
 */
static void add_hash(wordhash_t *dest, wordhash_t *src) {
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

/* read messages from stdin and register according to _run_type.
 *
 * performance cheat: we use a per-message hash and a global hash. After
 * each message, we accumulate the per-message frequencies in the global
 * hash. This may look long-winded, but is actually fast because it
 * saves us iterating over tokens with zero frequencies in the
 * cap-and-accumulation phase. we save more than half of the execution
 * time for big mbox inputs, when teaching bogofilter.
 */
rc_t register_messages()
{
  wordhash_t *words = wordhash_init();
  long	msgcount = 0;
  token_t token_type;

  initialize_constants();

  do {
      wordhash_t *h = wordhash_init();
      msgcount++;
      if (DEBUG_REGISTER(1))
	  fprintf(dbgout, "Message #%ld\n", msgcount);
      token_type = collect_words(h);
      wordhash_sort(h);
      add_hash(words, h);
      wordhash_free(h);
  } while (token_type != NONE);

  wordhash_sort(words);
  register_words(run_type, words, msgcount);
  wordhash_free(words);

  return RC_OK;
}
