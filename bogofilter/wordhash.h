#ifndef WORDHASH_H_GUARD
#define WORDHASH_H_GUARD

/* Hash entry. */
typedef struct hashnode_t {
  char *key;                              /* word key */
  void *buf;                              /* Associated buffer. To be used by caller. */
  struct hashnode_t *next;               /* Next item in linked list of items with same hash */ 
  struct hashnode_t *iter_next;          /* Next item added to hash. For fast traversal */
} hashnode_t;

/* Managed heap for memory  allocation */
typedef struct halloc_t {
  char *buf;
  int avail;
  int used;
  struct halloc_t *next;
} halloc_t;


/* hash table, with bookkeeping */
typedef struct {
  hashnode_t **bin;     /* hash table */
  halloc_t *halloc_buf; /*list of node buffers */

  hashnode_t *iter_ptr;    /* For traversal */
  hashnode_t *iter_head;
  hashnode_t *iter_tail;
  
} wordhash_t;

/* initialize a wordhash */
wordhash_t *wordhash_init(void);

/* deallocate resources */
void wordhash_free(wordhash_t *);

/* Given hash table h, key s, and int  n, search for key s.
* If found, return pointer to associated buffer,
  else, insert key and return pointer to allocated buffer of size n */
void *wordhash_insert(wordhash_t *, char *, size_t);

/* Starts an iteration over the hash entries */
hashnode_t *wordhash_first(wordhash_t *);

/* returns next entry or NULL if at end */
hashnode_t *wordhash_next(wordhash_t *);

#endif
