/* $Id$ */

#ifndef WORDHASH_H_GUARD
#define WORDHASH_H_GUARD

/* Hash entry. */
typedef struct hashnode_t {
  char *key;                              /* word key */
  void *buf;                              /* Associated buffer. To be used by caller. */
  struct hashnode_t *next;               /* Next item in linked list of items with same hash */ 
  /*@dependent@*/ struct hashnode_t *iter_next;          /* Next item added to hash. For fast traversal */
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

typedef struct {
  hashnode_pt *bin;
  /*@null@*/ /*@owned@*/ wh_alloc_node *nodes; /*list of node buffers */
  /*@null@*/  wh_alloc_str  *strings; /* list of string buffers */

  /*@null@*/  /*@dependent@*/ hashnode_t *iter_ptr;
  /*@null@*/  /*@dependent@*/ hashnode_t *iter_head;
  /*@null@*/  /*@dependent@*/ hashnode_t *iter_tail;

} wordhash_t;

/*@only@*/ wordhash_t *wordhash_init(void);

void wordhash_free(/*@only@*/ wordhash_t *);

/* Given h, s, n, search for key s.
 * If found, return pointer to associated buffer.
 * Else, insert key and return pointer to allocated buffer of size n. */
/*@observer@*/ void *wordhash_insert(wordhash_t *, char *, size_t, void (*)(void *));

/* Starts an iteration over the hash entries */
/*@null@*/ /*@exposed@*/ hashnode_t *wordhash_first(wordhash_t *);

/* returns next entry or NULL if at end */
/*@null@*/ /*@exposed@*/ hashnode_t *wordhash_next(wordhash_t *);

#endif
