/* register.c -- read input with collect and register to persistent db */
/* $Id$ */

#include <stdio.h>
#include <stdlib.h>

#include <config.h>
#include "common.h"

#include "bogofilter.h"
#include "datastore.h"
#include "register.h"
#include "collect.h"
#include "wordhash.h"

#define PLURAL(count) ((count == 1) ? "" : "s")

int	max_repeats;

extern char msg_register[];


/*
 * tokenize text on stdin and register it to a specified list
 * and possibly out of another list
 */
void register_words(run_t _run_type, wordhash_t *h,
		    int msgcount, int wordcount)
{
  char ch = '\0';
  hashnode_t *node;
  wordprop_t *wordprop;

  wordlist_t *list;
  wordlist_t *incr_list = NULL;
  wordlist_t *decr_list = NULL;

  switch(_run_type)
  {
    case REG_SPAM:		ch = 's' ;  break;
    case REG_GOOD:		ch = 'n' ;  break;
    case REG_GOOD_TO_SPAM:	ch = 'S' ;  break;
    case REG_SPAM_TO_GOOD:	ch = 'N' ;  break;
    default:			abort(); 
  }

  (void)sprintf(msg_register, "register-%c, %d words, %d messages\n", ch,
		wordcount, msgcount);

  if (verbose)
    (void)fprintf(stderr, "# %d word%s, %d message%s\n", 
		  wordcount, PLURAL(wordcount), msgcount, PLURAL(msgcount));

  good_list.active = spam_list.active = false;

  switch(_run_type)
  {
    case REG_GOOD:
      incr_list = &good_list;
      break;

    case REG_SPAM:
      incr_list = &spam_list;
      break;

    case  REG_GOOD_TO_SPAM:
      decr_list = &good_list;
      incr_list = &spam_list;
      break;

    case REG_SPAM_TO_GOOD:
      incr_list = &good_list;
      decr_list = &spam_list;
      break;

    default:
      abort();
  }

  incr_list->active = true;
  if (decr_list)
    decr_list->active = true;

  db_lock_writer_list(word_lists);

  for (list = word_lists; list != NULL; list = list->next){
    if (list->active) {
      list->msgcount = db_getcount(list->dbh);
    }
  }

  incr_list->msgcount += msgcount;

  if (decr_list){
    if (decr_list->msgcount > msgcount)
      decr_list->msgcount -= msgcount;
    else
      decr_list->msgcount = 0;
  }

  for (node = wordhash_first(h); node != NULL; node = wordhash_next(h)){
    wordprop = node->buf;
    db_increment(incr_list->dbh, node->key, wordprop->freq);
    if (decr_list) db_decrement(decr_list->dbh, node->key, wordprop->freq);
  }

  for (list = word_lists; list != NULL; list = list->next){
    if (list->active) {
      db_setcount(list->dbh, list->msgcount);
      db_flush(list->dbh);
      if (verbose>1)
	(void)fprintf(stderr, "bogofilter: %ld messages on the %s list\n",
		      list->msgcount, list->name);
    }
  }

  db_lock_release_list(word_lists);
}

/* this function accumulates the word frequencies from the src hash to
 * those of the dest hash
 */
static void add_hash(wordhash_t *dest, wordhash_t *src) {
    wordprop_t *d;
    hashnode_t *s;

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
  wordhash_t *h, *words = wordhash_init();
  long	wordcount, totalwords = 0, msgcount = 0;
  bool cont;

  initialize_constants();

  do {
    collect_words(&h, &wordcount, &cont);
    add_hash(words, h);
    wordhash_free(h);
    msgcount++;
    totalwords += wordcount;
  } while(cont);

  register_words(_run_type, words, msgcount, totalwords);
  wordhash_free(words);
}
