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

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <stddef.h>	/* for offsetof */

#include "wordhash.h"
#include "xmalloc.h"

#define NHASH 29989
#define MULT 31

/* Note:  every wordhash includes three large chunks of memory:
   20k - S_CHUNK -- 
   32k - N_CHUNK * sizeof (hashnode_t)
   120k - NHASH * sizeof (hashnode_t **)
*/

#define N_CHUNK 2000
#define S_CHUNK 20000

#ifndef offsetof
#define offsetof(type, member) ((size_t) &((type*)0)->member )
#endif

/* 06/14/03
** profiling shows wordhash_new() is significant portion 
** of time spent processing msg-count files:
**
**   %   cumulative   self              self     total           
**  time   seconds   seconds    calls  ms/call  ms/call  name    
**  30.84      0.70     0.70     2535     0.28     0.28  wordhash_new
**
** wordhash_new() made faster by using xcalloc() to provide
** initialized storage.
*/

wordhash_t *
wordhash_new (void)
{
  wordhash_t *wh = xcalloc (1, sizeof (wordhash_t));
  wh->bin = xcalloc (NHASH, sizeof (hashnode_t **));
  return wh;
}

static void
wordhash_free_alloc_nodes (wordhash_t *wh)
{
    wh_alloc_node *node, *next;

    /* iteration pointers are inside buf,
    ** so freeing buf requires clearing them.
    */
    wh->iter_head = wh->iter_tail = NULL;

    for (node = wh->nodes; node; node = next)
    {
	next = node->next;
	xfree (node->buf);
	xfree (node);
    }
    wh->nodes = NULL;
}

static void
wordhash_free_hash_nodes (wordhash_t *wh)
{
    hashnode_t *item, *next;

    for (item = wh->iter_head; item != NULL ; item = next)
    {
	next = item->iter_next;
	word_free( item->key );
	item->key = NULL;
    }
    wh->iter_head = NULL;
}

static void
wordhash_free_strings (wordhash_t *wh)
{
    wh_alloc_str *sp, *sq;

    for (sp = wh->strings; sp; sp = sq)
    {
	sq = sp->next;
	xfree (sp->buf);
	xfree (sp);
    }
    wh->strings = NULL;
}

void
wordhash_free (wordhash_t *wh)
{
    if (wh == NULL)
	return;

    wordhash_free_hash_nodes(wh);
    wordhash_free_alloc_nodes(wh);
    wordhash_free_strings(wh);

    xfree (wh->order);
    xfree (wh->props);
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

static void display_node(hashnode_t *n, const char *str)
{
    wordprop_t *p = (wordprop_t *)n->buf;
    if (verbose > 2)
	printf( "%20.20s %5d %5d%s", n->key->text, (int)p->cnts.bad, (int)p->cnts.good, str);
}

/* this function accumulates the word frequencies from the src hash to
 * those of the dest hash
 */
void wordhash_add(wordhash_t *dest, wordhash_t *src, void (*initializer)(void *))
{
    wordprop_t *d;
    hashnode_t *s;

    uint count = dest->count + src->count;	/* use dest count as total */

    dest->wordcount += src->wordcount;

    if (verbose > 20) {
	printf( "%5lu  ", (unsigned long)dest->wordcount);
	display_node(src->iter_head, "  ");
    }

    for (s = wordhash_first(src); s != NULL; s = wordhash_next(src)) {
	wordprop_t *p = (wordprop_t *)s->buf;
	word_t *key = s->key;
	if (key == NULL)
	    continue;
	d = wordhash_insert(dest, key, sizeof(wordprop_t), initializer);
	d->freq += p->freq;
	d->cnts.good += p->cnts.good;
	d->cnts.bad  += p->cnts.bad;
    }

    if (verbose > 200)
	display_node(dest->iter_head, "\n");

    dest->count = count;
}

void
wordhash_foreach (wordhash_t *wh, wh_foreach_t *hook, void *userdata)
{
    hashnode_t *hn;

    for (hn = wordhash_first(wh); hn != NULL; hn = wordhash_next(wh)) {
	(*hook)(hn->key, hn->buf, userdata);
    }

    return;
}

void *
wordhash_search (wordhash_t *wh, word_t *t, unsigned int idx)
{
    hashnode_t *hn;

    if (idx == 0)
	idx = hash (t);

    for (hn = wh->bin[idx]; hn != NULL; hn = hn->next) {
	word_t *key = hn->key;
	if (key->leng == t->leng && memcmp (t->text, key->text, t->leng) == 0)
	{
	    return hn->buf;
	}
    }
    return NULL;
}

void *
wordhash_insert (wordhash_t *wh, word_t *t, size_t n, void (*initializer)(void *))
{
    hashnode_t *hn;
    unsigned int idx = hash (t);
    void *buf = wordhash_search(wh, t, idx);

    if (buf != NULL)
	return buf;

    wh->count += 1;
    hn = nmalloc (wh);
    hn->buf = smalloc (wh, n);
    if (initializer)
	initializer(hn->buf);
    else
	memset(hn->buf, '\0', n);

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

void *
wordhash_first (wordhash_t *wh)
{
    void *val = NULL;

    if (wh->cnts) {
	wh->index = 0;
	val = wh->cnts;
    }
    else
    if (wh->order) {
	wh->index = 0;
	val = wh->order[wh->index];
    }
    else {
	val = wh->iter_ptr = wh->iter_head;
    }
    return val;
}

void *
wordhash_next (wordhash_t *wh)
{
    void *val = NULL;

    if (wh->cnts) {
	if (++wh->index < wh->count)
	    val = &wh->cnts[wh->index];
    }
    else
    if (wh->order) {
	if (++wh->index < wh->count)
	    val = wh->order[wh->index];
    }
    else {
	if (wh->iter_ptr != NULL)
	    val = wh->iter_ptr = wh->iter_ptr->iter_next;
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
wordhash_set_counts(wordhash_t *wh, int good, int bad)
{
    hashnode_t *n;

    for (n = wordhash_first(wh); n != NULL; n = wordhash_next(wh)) {
	wordprop_t *p = (wordprop_t *)n->buf;
	p->cnts.good += good;
	p->cnts.bad  += bad;
    }
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

/* 
** wordhash_convert_to_countlist() allocates a new wordhash_t struct
** to improve program locality and lessen need for swapping when
** processing lots of messages.
*/

wordhash_t *
wordhash_convert_to_countlist(wordhash_t *whi, wordhash_t *db)
{
    hashnode_t *node;
    wordhash_t *who = wordhash_new();

    who->cnts = (wordcnts_t *) xcalloc(whi->count, sizeof(wordcnts_t));

    for(node = wordhash_first(whi); node != NULL; node = wordhash_next(whi)) {
	wordprop_t *wp;
	wordcnts_t *cnts = &who->cnts[who->count];
	if (!msg_count_file)
	    wp = wordhash_insert(db, node->key, sizeof(wordprop_t), NULL);
	else
	    wp = (wordprop_t *) node->buf;
	cnts->good = wp->cnts.good;
	cnts->bad  = wp->cnts.bad ;
	who->count += 1;
/*
	word_free(node->key);
	node->key = NULL;
*/
    }

    xfree(who->bin);		/* discard extra storage */
    who->bin = NULL;

    return who;
}
