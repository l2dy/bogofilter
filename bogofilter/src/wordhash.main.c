/* $Id$ */

#include "common.h"

#include "wordhash.h"

typedef struct
{
  int count;
}
wh_elt_t;

/* dummy function to satisfy reference in wordhash_degen() */
void degen(word_t *token, wordcnts_t *cnts);
void degen(word_t *token, wordcnts_t *cnts)
{
    token = NULL;	/* quiet compiler */
    cnts  = NULL;	/* quiet compiler */
    return;
}

/* function definitions */

static void word_init(void *vw){
     wh_elt_t *w = vw;
     w->count = 0;   
}

void dump_hash (wordhash_t *);

void
dump_hash (wordhash_t * h)
{
  hashnode_t *p;
  for (p = wordhash_first (h); p != NULL; p = wordhash_next (h))
    {
      word_t *key = p->key;
      (void)word_puts(key, 0, stdout);
      (void)printf (" %d\n", ((wh_elt_t *) p->buf)->count);
    }
}

int
main (void)
{
  wordhash_t *h = wordhash_new ();
  char buf[100];
  wh_elt_t *w;

  while (scanf ("%99s", buf) != EOF)
    {
      word_t *t = word_new((byte *)buf, strlen(buf));
      w = wordhash_insert (h, t, sizeof (word_t), &word_init);
      w->count++;
    }

  dump_hash (h);
  wordhash_free (h);
  return 0;
}
