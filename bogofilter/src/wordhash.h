/* $Id$ */

#ifndef WORDHASH_H
#define WORDHASH_H

#include "word.h"

/* Hash entry. */
typedef struct hashnode_t {
  word_t *key;					/* word key */
  void   *buf;					/* Associated buffer. To be used by caller. */
  struct hashnode_t *next;			/* Next item in linked list of items with same hash */ 
  /*@dependent@*/ struct hashnode_t *iter_next;	/* Next item added to hash. For fast traversal */
} hashnode_t;

typedef struct wh_alloc_node {
  hashnode_t *buf;
  /*@refs@*/ size_t avail;
  /*@refs@*/ size_t used;
  /*@null@*/ struct wh_alloc_node *next;
} wh_alloc_node;

typedef struct wh_alloc_str {
  char *buf;
  /*@refs@*/ size_t avail;
  /*@refs@*/ size_t used;
  /*@null@*/ struct wh_alloc_str *next;
} wh_alloc_str;

typedef /*@null@*/ hashnode_t *hashnode_pt;

typedef struct wordhash_s {
  hashnode_pt *bin;
  /*@null@*/ /*@owned@*/ wh_alloc_node *nodes;		/*list of node buffers */
  /*@null@*/  		 wh_alloc_str  *strings;	/* list of string buffers */

  /*@null@*/  /*@dependent@*/ hashnode_t *iter_ptr;
  /*@null@*/  /*@dependent@*/ hashnode_t *iter_head;
  /*@null@*/  /*@dependent@*/ hashnode_t *iter_tail;

  /*@null@*/  /*@dependent@*/ uint index;		/* access index */
  /*@null@*/  /*@dependent@*/ uint count;		/* size of array */
  /*@null@*/  /*@dependent@*/ uint wordcount;		/* count of words */
  /*@null@*/  /*@dependent@*/ hashnode_t **order;	/* array of nodes */
  /*@null@*/  /*@dependent@*/ hashnode_t  *props;	/* array of nodes */
} wordhash_t;

/*@only@*/ wordhash_t *wordhash_new(void);

void wordhash_free(/*@only@*/ wordhash_t *);
size_t wordhash_count(wordhash_t * h);
void wordhash_sort(wordhash_t * h);
void wordhash_add(wordhash_t *dst, wordhash_t *src, void (*initializer)(void *));
void wordhash_set_counts(wordhash_t *wh, int good, int bad);

void * wordhash_search (wordhash_t *wh, word_t *t, unsigned int hash);

/* Given h, s, n, search for key s.
 * If found, return pointer to associated buffer.
 * Else, insert key and return pointer to allocated buffer of size n. */
/*@observer@*/ void *wordhash_insert(wordhash_t *, word_t *, size_t, void (*)(void *));

/* Starts an iteration over the hash entries */
/*@null@*/ /*@exposed@*/ hashnode_t *wordhash_first(wordhash_t *);

/* returns next entry or NULL if at end */
/*@null@*/ /*@exposed@*/ hashnode_t *wordhash_next(wordhash_t *);

typedef void wh_foreach_t(word_t *token, void *data, void *userdata);
void wordhash_foreach(wordhash_t *wh, wh_foreach_t *hook, void *userdata);
void wordhash_convert_to_proplist(wordhash_t *wh, wordhash_t *db);

#endif
