#ifndef COLLECT_H
#define COLLECT_H

/* Represents the secondary data for a word key */
typedef struct {
  int freq;
} wordprop_t;

extern void wordprop_init(void *vwordprop);

extern void collect_words(/*@out@*/ wordhash_t **wh,
       /*@out@*/ /*@null@*/ long *word_count, /*@out@*/ bool *cont);

extern void collect_reset(void);

#endif
