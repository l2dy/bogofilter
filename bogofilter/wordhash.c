/*
$Id$
$Log$
Revision 1.1  2002/09/29 03:37:56  gyepi
replace Judy with hash table (wordhash)

*/

/*
NAME:
   wordhash.c -- Implements a hash data structure.

AUTHOR:
   Gyepi Sam <gyepi@praxis-sw.com>

THEORY:
  See 'Programming Pearls' by Jon Bentley for a good treatment of word hashing.
  The algorithm, magic number selections, and hash function are based on his implementation.

  This module has been tuned to perform fast inserts and searches, using the following techniques:

  1. Multiple, sequential memory allocation operations are combined into a single call.
     The allocated memory is then divided as necessary.
  2. Insert operation allocates and returns a pointer to a memory buffer, in which the
     caller can store associated data, eliminating the need for further  retrieval or storage operations.
  3. Maintains a linked list of hash nodes in insert order for fast traversal of hash table.
*/


#ifdef MAIN
#include <stdio.h>
#endif

#include <stdlib.h>
#include <string.h>
#include "xmalloc.h"
#include "wordhash.h"

#define NHASH 29989
#define MULT 31

#define CHUNK_SIZE 30000

wordhash_t *wordhash_init(void)
{
    int i;
    wordhash_t *h = xmalloc(sizeof(wordhash_t));
    h->bin = xmalloc(NHASH * sizeof(hashnode_t **));

    for (i = 0; i < NHASH; i++)
	h->bin[i] = NULL;

    h->halloc_buf = NULL;

    h->iter_ptr = NULL;
    h->iter_head = NULL;
    h->iter_tail = NULL;

    return h;
}

void wordhash_free(wordhash_t * h)
{
    halloc_t *p, *q;

    if (h == NULL)
	return;

    for (p = h->halloc_buf; p != NULL; p = q) {
	q = p->next;
	xfree(p);
    }

    xfree(h->bin);
    xfree(h);
}

static void *hmalloc(wordhash_t * h, size_t n)
{
    halloc_t *x = h->halloc_buf;
    char *t;

    if (x == NULL || x->avail < n) {
	/* Eliminate extra call to xmalloc by allocating
	   enough data for both the node and its buffer */
	t = xmalloc(sizeof(halloc_t) + CHUNK_SIZE + n);

	x = (halloc_t *) t;
	x->next = h->halloc_buf;
	h->halloc_buf = x;

	x->buf = t + sizeof(halloc_t);
	x->avail = CHUNK_SIZE + n;
	x->used = 0;
    }

    x->avail -= n;
    t = x->buf + x->used;
    x->used += n;
    return (void *) t;
}

static unsigned int hash(char *p)
{
    unsigned int h = 0;
    for (; *p; p++)
	h = MULT * h + *p;
    return h % NHASH;
}

void *wordhash_insert(wordhash_t * h, char *s, size_t n)
{
    hashnode_t *p;
    size_t m;
    int index = hash(s);
    char *t;

    for (p = h->bin[index]; p != NULL; p = p->next)
	if (strcmp(s, p->key) == 0) {
	    return p->buf;
	}

    m = strlen(s) + 1;

    t = hmalloc(h, sizeof(hashnode_t) + n + m);;

    p = (hashnode_t *) t;

    p->buf = t + sizeof(hashnode_t);

    p->key = t + sizeof(hashnode_t) + n;

    memcpy(p->key, s, m);

    p->next = h->bin[index];
    h->bin[index] = p;

    if (h->iter_head == NULL) {
	h->iter_head = p;
    } else {
	h->iter_tail->iter_next = p;
    }

    p->iter_next = NULL;
    h->iter_tail = p;

    return p->buf;
}

hashnode_t *wordhash_first(wordhash_t * h)
{
    return (h->iter_ptr = h->iter_head);
}

hashnode_t *wordhash_next(wordhash_t * h)
{
    if (h->iter_ptr != NULL)
	h->iter_ptr = h->iter_ptr->iter_next;

    return h->iter_ptr;
}

#ifdef MAIN

typedef struct {
    int count;
} word_t;

void dump_hash(wordhash_t * h)
{
    hashnode_t *p;
    for (p = wordhash_first(h); p != NULL; p = wordhash_next(h)) {
	printf("%s %d\n", p->key, ((word_t *) p->buf)->count);
    }
}

int main()
{
    wordhash_t *h = wordhash_init();
    char buf[100];
    word_t *w;

    while (scanf("%s", buf) != EOF) {
	w = wordhash_insert(h, buf, sizeof(word_t));
	w->count++;
    }

    dump_hash(h);
    wordhash_free(h);
    return 0;
}
#endif
