/* $Id$ */

/*
NAME:
   wordhash.c -- Implements a hash data structure.

AUTHOR:
   Gyepi Sam <gyepi@praxis-sw.com>

THEORY:
  See 'Programming Pearls' by Jon Bentley for a good treatment of word hashing.
  The algorithm, magic number selections, and hash function are based on
  his implementation.

  This module has been tuned to perform fast inserts and searches, using
  the following techniques:

  1. Insert operation allocates and returns a pointer to a memory
  buffer, in which the caller can store associated data, eliminating the
  need for further  retrieval or storage operations.

  2. Maintains a linked list of hash nodes in insert order for fast
  traversal of hash table.
*/

#ifdef MAIN
#include <stdio.h>
#endif

#include <stdlib.h>
#include <string.h>

/* for offsetof */
#include <stddef.h>

#include "xmalloc.h"
#include "wordhash.h"

#define NHASH 29989
#define MULT 31

#define N_CHUNK 2000
#define S_CHUNK 20000

#ifndef offsetof
#define offsetof(type, member) ( (int) & ((type*)0) -> member )
#endif

wordhash_t *
wordhash_init (void)
{
  int i;

  wordhash_t *h = xmalloc (sizeof (wordhash_t));

  h->bin = xmalloc (NHASH * sizeof (hashnode_t **));

  for (i = 0; i < NHASH; i++)
    h->bin[i] = NULL;

  h->nodes = NULL;
  h->strings = NULL;

  h->iter_ptr = NULL;
  h->iter_head = NULL;
  h->iter_tail = NULL;

  return h;
}

void
wordhash_free (wordhash_t * h)
{
  wh_alloc_node *np, *nq;
  wh_alloc_str *sp, *sq;

  if (h == NULL)
    return;

  for (np = h->nodes; np; np = nq)
    {
      nq = np->next;
      xfree (np->buf);
      xfree (np);
    }

  for (sp = h->strings; sp; sp = sq)
    {
      sq = sp->next;
      xfree (sp->buf);
      xfree (sp);
    }

  xfree (h->bin);
  xfree (h);
}

static hashnode_t *
nmalloc (wordhash_t * h) /*@modifies h->nodes@*/
{
  /*@dependent@*/ wh_alloc_node *x = h->nodes;
  hashnode_t *t;

  if (x == NULL || x->avail == 0)
    {

      x = xmalloc (sizeof (wh_alloc_node));
      x->next = h->nodes;
      h->nodes = x;

      x->buf = xmalloc (N_CHUNK * sizeof (hashnode_t));
      x->avail = N_CHUNK;
      x->used = 0;
    }
  x->avail--;
  t = (x->buf) + x->used;
  x->used ++;
  return (t);
}

/* determine architecture's alignment boundary */
struct dummy { char *c; int  i; };
#define ALIGNMENT offsetof(struct dummy, i)

static char *
smalloc (wordhash_t * h, size_t n) /*@modifies h->strings@*/
{
  wh_alloc_str *x = h->strings;
  /*@dependent@*/ char *t;

  /* Force alignment on architecture's natural boundary.*/
  n += ALIGNMENT - ( n % ALIGNMENT);
   
  if (x == NULL || x->avail < n)
    {
      x = xmalloc (sizeof (wh_alloc_str));
      x->next = h->strings;
      h->strings = x;

      x->buf = xmalloc (S_CHUNK + n);
      x->avail = S_CHUNK + n;
      x->used = 0;
    }
  x->avail -= n;
  t = x->buf + x->used;
  x->used += n;
  return (t);
}

static unsigned int
hash (char *p)
{
  unsigned int h = 0;
  for (; *p; p++)
    h = MULT * h + *p;
  return h % NHASH;
}

void *
wordhash_insert (wordhash_t * h, char *s, size_t n, void (*initializer)(void *))
{
  hashnode_t *p;
  unsigned int index = hash (s);

  for (p = h->bin[index]; p != NULL; p = p->next)
    if (strcmp (s, p->key) == 0)
      {
	return p->buf;
      }

  p = nmalloc (h);
  p->buf = smalloc (h, n);
  if (initializer)
	  initializer(p->buf);
		  
  p->key = smalloc (h, strlen (s) + 1);
  strcpy (p->key, s);
  p->next = h->bin[index];
  h->bin[index] = p;

  if (h->iter_head == NULL){
    h->iter_head = p;
  }
  else {
    h->iter_tail->iter_next = p;
  }

  p->iter_next = NULL;
  h->iter_tail = p;

  return p->buf;
}

hashnode_t *
wordhash_first (wordhash_t * h)
{
  return(h->iter_ptr = h->iter_head);
}

hashnode_t *
wordhash_next (wordhash_t * h)
{
  if (h->iter_ptr != NULL)
    h->iter_ptr = h->iter_ptr->iter_next;
   
  return h->iter_ptr;
}

