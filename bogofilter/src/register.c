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

  int wordcount = h->wordcount;

  wordlist_t *list;
  wordlist_t *incr_list = NULL;
  wordlist_t *decr_list = NULL;

  /* If update directory explicity supplied, setup the wordlists. */
  if (update_dir) {
      if (setup_wordlists(update_dir, PR_CFG_UPDATE) != 0)
	  exit(2);
  }

  if (_run_type & REG_SPAM) r = "s";
  if (_run_type & REG_GOOD) r = "n";
  if (_run_type & UNREG_SPAM) u = "S";
  if (_run_type & UNREG_GOOD) u = "N";

  if (wordcount == 0)
      msgcount = 0;

  format_log_update(msg_register, msg_register_size, u, r, wordcount, msgcount);

  if (verbose)
    (void)fprintf(stderr, "# %d word%s, %d message%s\n", 
		  wordcount, PLURAL(wordcount), msgcount, PLURAL(msgcount));

  set_list_active_status(false);

  if (_run_type & REG_GOOD) incr_list = good_list;
  if (_run_type & REG_SPAM) incr_list = spam_list;
  if (_run_type & UNREG_GOOD) decr_list = good_list;
  if (_run_type & UNREG_SPAM) decr_list = spam_list;

  if (DEBUG_REGISTER(2))
      fprintf(dbgout, "%s%s -- incr: %08lX, decr: %08lX\n", r, u,
	      (unsigned long)incr_list, (unsigned long)decr_list);

  if (incr_list)
    incr_list->active = true;
  if (decr_list)
    decr_list->active = true;

  for (list = word_lists; list != NULL; list = list->next){
    if (list->active) {
      list->msgcount = db_get_msgcount(list->dbh);
    }
  }

  if (incr_list) incr_list->msgcount += msgcount;

  if (decr_list) {
    if (decr_list->msgcount > msgcount)
      decr_list->msgcount -= msgcount;
    else
      decr_list->msgcount = 0;
  }

  for (node = wordhash_first(h); node != NULL; node = wordhash_next(h)){
    wordprop = node->buf;
    if (incr_list) db_increment(incr_list->dbh, node->key, wordprop->freq);
    if (decr_list) db_decrement(decr_list->dbh, node->key, wordprop->freq);
  }

  for (list = word_lists; list != NULL; list = list->next){
    if (list->active) {
      db_set_msgcount(list->dbh, list->msgcount);
      db_flush(list->dbh);
      if (verbose>1)
	(void)fprintf(stderr, "bogofilter: %ld messages on the %s list\n",
		      list->msgcount, list->filename);
    }
  }
}

/* this function accumulates the word frequencies from the src hash to
 * those of the dest hash
 */
static void add_hash(wordhash_t *dest, wordhash_t *src) {
    wordprop_t *d;
    hashnode_t *s;

    dest->wordcount += src->wordcount;

    for (s = wordhash_first(src); s; s = wordhash_next(src)) {
	d = wordhash_insert(dest, s->key, sizeof(wordprop_t), &wordprop_init);
	d -> freq += ((wordprop_t *)(s -> buf)) ->freq;
    }
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
void register_messages(run_t _run_type)
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
  register_words(_run_type, words, msgcount);
  wordhash_free(words);
}
