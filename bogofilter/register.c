/* $Id$ */

#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "common.h"
#include "bogofilter.h"
#include "datastore.h"
#include "lexer.h"
#include "register.h"
#include "wordhash.h"
#include "globals.h"

#define PLURAL(count) ((count == 1) ? "" : "s")

int	max_repeats;

extern char msg_register[];

/* Represents the secondary data for a word key */
typedef struct {
  int freq;
  int msg_freq;
} wordprop_t;

static void wordprop_init(void *vwordprop){
	wordprop_t *wordprop = vwordprop;

	wordprop->freq = 0;
	wordprop->msg_freq = 0;
}

void *collect_words(/*@out@*/ int *message_count,
		    /*@out@*/ int *word_count)
    /* tokenize input text and save words in wordhash_t hash table 
     * returns: the wordhash_t hash table.
     * Sets messageg_count and word_count to the appropriate values
     * if their pointers are non-NULL.  */
{
  int w_count = 0;
  int msg_count = 0;
 
  wordprop_t *w;
  hashnode_t *n;
  wordhash_t *h = wordhash_init();
     
  for (;;){
    token_t token_type = get_token();
  
    if (token_type != FROM && token_type != 0){
      w = wordhash_insert(h, yylval, sizeof(wordprop_t), &wordprop_init);
      w->msg_freq++;
      w_count++;
    }
    else {
      /* End of message. Update message counts. */
      if (token_type == FROM || (token_type == 0 && msg_count == 0))
        msg_count++;
  
      /* Incremenent word frequencies, capping each message's
       * contribution at MAX_REPEATS in order to be able to cap
       * frequencies. */
      for(n = wordhash_first(h); n != NULL; n = wordhash_next(h)){
        w = n->buf;
        if (w->msg_freq > max_repeats)
          w->msg_freq = max_repeats;
        w->freq += w->msg_freq;
        w->msg_freq = 0;
      }
  
      /* Want to process EOF, *then* drop out */
      if (token_type == 0)
        break;
    }
  }
 
  if (word_count)
    *word_count = w_count;

  if (message_count)
    *message_count = msg_count;
 
  return(h);
}


void register_words(run_t _run_type, wordhash_t *h,
		    int msgcount, int wordcount)
/* tokenize text on stdin and register it to  a specified list
 * and possibly out of another list
 */
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

  good_list.active = spam_list.active = FALSE;

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
      (void)fprintf(stderr, "Error: Invalid run_type\n");
      exit(2);      
    }

  incr_list->active = TRUE;
  if (decr_list)
    decr_list->active = TRUE;

  db_lock_writer_list(word_lists);

  for (list = word_lists; list != NULL; list = list->next){
    if (list->active == TRUE) {
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

void register_messages(run_t _run_type)
{
  wordhash_t *h;
  int	wordcount, msgcount;
  initialize_constants();
  h = collect_words(&msgcount, &wordcount);
  register_words(_run_type, h, msgcount, wordcount);
  wordhash_free(h);
}

