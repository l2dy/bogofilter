#include <stdio.h>

#include "wordhash.h"

typedef struct
{
  int count;
}
word_t;

static void word_init(void *vw){
     word_t *w = vw;
     w->count = 0;   
}

void dump_hash (wordhash_t *);

void
dump_hash (wordhash_t * h)
{
  hashnode_t *p;
  for (p = wordhash_first (h); p != NULL; p = wordhash_next (h))
    {
      (void)printf ("%s %d\n", p->key, ((word_t *) p->buf)->count);
    }
}

int
main (void)
{
  wordhash_t *h = wordhash_init ();
  char buf[100];
  word_t *w;

  while (scanf ("%99s", buf) != EOF)
    {
      w = wordhash_insert (h, buf, sizeof (word_t), &word_init);
      w->count++;
    }

  dump_hash (h);
  wordhash_free (h);
  return 0;
}
