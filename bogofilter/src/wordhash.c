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

#include <stdlib.h>
#include <string.h>

/* for offsetof */
#include <stddef.h>

#include "common.h"

#include "wordhash.h"
#include "xmalloc.h"

#define NHASH 29989
#define MULT 31

#define N_CHUNK 2000
#define S_CHUNK 20000

#ifndef offsetof
#define offsetof(type, member) ( (int) & ((type*)0) -> member )
#endif

/* 06/14/03
** profiling shows wordhash_init() is significant portion 
** of time spent processing msg-count files:
**
**   %   cumulative   self              self     total           
**  time   seconds   seconds    calls  ms/call  ms/call  name    
**  30.84      0.70     0.70     2535     0.28     0.28  wordhash_init
**
** wordhash_init() made faster by using xcalloc() to provide
** initialized storage.
*/

wordhash_t *
wordhash_init (void)
{
  wordhash_t *wh = xcalloc (1, sizeof (wordhash_t));
  wh->bin = xcalloc (NHASH, sizeof (hashnode_t **));
  return wh;
}

void
wordhash_free (wordhash_t *wh)
{
    hashnode_t *p, *q;
    wh_alloc_str *sp, *sq;
    wh_alloc_node *np, *nq;

    if (wh == NULL)
	return;

    for (p = wh->iter_head; p != NULL ; p = q)
    {
	q = p->iter_next;
	word_free( p->key );
    }

    for (np = wh->nodes; np; np = nq)
    {
	nq = np->next;
	xfree (np->buf);
	xfree (np);
    }

    for (sp = wh->strings; sp; sp = sq)
    {
	sq = sp->next;
	xfree (sp->buf);
	xfree (sp);
    }

    if (wh->order != NULL) {
	xfree (wh->order);
    }

    xfree (wh->bin);
    xfree (wh);
}

static hashnode_t *
nmalloc (wordhash_t *wh) /*@modifies wh->nodes@*/
{
    /*@dependent@*/ wh_alloc_node *wn = wh->nodes;
    hashnode_t *hn;

    if (wn == NULL || wn->avail == 0)
    {
	wn = xmalloc (sizeof (wh_alloc_node));
	wn->next = wh->nodes;
	wh->nodes = wn;

	wn->buf = xmalloc (N_CHUNK * sizeof (hashnode_t));
	wn->avail = N_CHUNK;
	wn->used = 0;
    }
    wn->avail--;
    hn = (wn->buf) + wn->used;
    wn->used ++;
    return (hn);
}

/* determine architecture's alignment boundary */
struct dummy { char *c; double d; };
#define ALIGNMENT offsetof(struct dummy, d)

static char *
smalloc (wordhash_t *wh, size_t n) /*@modifies wh->strings@*/
{
    wh_alloc_str *s = wh->strings;
    /*@dependent@*/ char *t;

    /* Force alignment on architecture's natural boundary.*/
    if ((n % ALIGNMENT) != 0)
	n += ALIGNMENT - ( n % ALIGNMENT);
   
    if (s == NULL || s->avail < n)
    {
	s = xmalloc (sizeof (wh_alloc_str));
	s->next = wh->strings;
	wh->strings = s;

	s->buf = xmalloc (S_CHUNK + n);
	s->avail = S_CHUNK + n;
	s->used = 0;
    }
    s->avail -= n;
    t = s->buf + s->used;
    s->used += n;
    return (t);
}

static unsigned int
hash (word_t *t)
{
    unsigned int h = 0;
    size_t l;
    for (l=0; l<t->leng; l++)
	h = MULT * h + t->text[l];
    return h % NHASH;
}

void *
wordhash_insert (wordhash_t *wh, word_t *t, size_t n, void (*initializer)(void *))
{
    hashnode_t *hn;
    unsigned int idx = hash (t);

    for (hn = wh->bin[idx]; hn != NULL; hn = hn->next) {
	word_t *key = hn->key;
	if (key->leng == t->leng && memcmp (t->text, key->text, t->leng) == 0)
	{
	    return hn->buf;
	}
    }

    wh->count += 1;
    hn = nmalloc (wh);
    hn->buf = smalloc (wh, n);
    if (initializer)
	initializer(hn->buf);
		  
    hn->key = word_dup(t);

    hn->next = wh->bin[idx];
    wh->bin[idx] = hn;

    if (wh->iter_head == NULL){
	wh->iter_head = hn;
    }
    else {
	wh->iter_tail->iter_next = hn;
    }

    hn->iter_next = NULL;
    wh->iter_tail = hn;

    return hn->buf;
}

size_t wordhash_count (wordhash_t *wh)
{
    return wh->count;
}

hashnode_t *
wordhash_first (wordhash_t *wh)
{
    hashnode_t *val = NULL;

    if (wh->order) {
	val = wh->order[wh->index = 0];
    }
    else {
	val = wh->iter_ptr = wh->iter_head;
    }
    return val;
}

hashnode_t *
wordhash_next (wordhash_t *wh)
{
    hashnode_t *val = NULL;

    if (wh->order) {
	if (++wh->index < wh->count) {
	    val = wh->order[wh->index];
	}
    }
    else {
	if (wh->iter_ptr != NULL) {
	    val = wh->iter_ptr = wh->iter_ptr->iter_next;
	}
    }
    return val;
}

static int compare_hashnode_t(const void *const ihn1, const void *const ihn2)
{
    const hashnode_t *hn1 = *(const hashnode_t *const *)ihn1;
    const hashnode_t *hn2 = *(const hashnode_t *const *)ihn2;
    return word_cmp(hn1->key, hn2->key);
}

void
wordhash_sort (wordhash_t *wh)
{
    hashnode_t *node;
    hashnode_t **order;

    if (msg_count_file)
	return;

    if (wh->count == 0 || wh->order != NULL)
	return;

    order = (hashnode_t **) xcalloc(wh->count, sizeof(hashnode_t *));

    wh->count = 0;
    for(node = wordhash_first(wh); node != NULL; node = wordhash_next(wh))
	order[wh->count++] = node;

    qsort(order, wh->count, sizeof(hashnode_t *), compare_hashnode_t);
    wh->order = order;
}
