/* $Id$ */

#ifndef WORDHASH_H_GUARD
#define WORDHASH_H_GUARD

/* Hash entry. */
typedef struct hashnode_t {
  char *key;                              /* word key */
  void *buf;                              /* Associated buffer. To be used by caller. */
  struct hashnode_t *next;               /* Next item in linked list of items with same hash */ 
  struct hashnode_t *iter_next;          /* Next item added to hash. For fast traversal */
} hashnode_t;

typedef struct wh_alloc_node {
  hashnode_t *buf;
  int avail;
  int used;
  struct wh_alloc_node *next;
} wh_alloc_node;

typedef struct wh_alloc_str {
  char * buf;
  size_t avail;
  int used;
  struct wh_alloc_str *next;
} wh_alloc_str;
 
typedef struct {
  hashnode_t **bin;
  wh_alloc_node *nodes; /*list of node buffers */
  wh_alloc_str  *strings; /* list of string buffers */

  size_t alignment;			 /* Architecture's alignment boundary */

  hashnode_t *iter_ptr;
  hashnode_t *iter_head;
  hashnode_t *iter_tail;

} wordhash_t;

wordhash_t *wordhash_init(void);

void wordhash_free(wordhash_t *);

/* Given h, s, n, search for key s. If found, return pointer to associated buffer.
   Else, insert key and return pointer to allocated buffer of size n */
void *wordhash_insert(wordhash_t *, char *, size_t, void (*)(void *));

/* Starts an iteration over the hash entries */
hashnode_t *wordhash_first(wordhash_t *);

/* returns next entry or NULL if at end */
hashnode_t *wordhash_next(wordhash_t *);

#endif
