/* $Id$ */

/*****************************************************************************

NAME:
   textblock.c -- implementation of textblock linked lists.

******************************************************************************/

#include <config.h>
#include "common.h"

#include "textblock.h"
#include "xmalloc.h"

/* Global Variables */

textblock_t *textblocks = NULL;

size_t cur_mem, max_mem, tot_mem;

/* Function Definitions */

textblock_t *textblock_init(void)
{
    textblock_t *t = (textblock_t *) xcalloc(1, sizeof(*t));
    size_t mem = sizeof(*t)+sizeof(textdata_t);
    t->head = (textdata_t *) xcalloc(1, sizeof(textdata_t));
    t->tail = t->head;
    cur_mem += mem;
    tot_mem += mem;
    max_mem = max(max_mem, cur_mem);
    if (DEBUG_TEXT(2)) fprintf(dbgout, "%s:%d  %p %p %3d alloc, cur: %d, max: %d, tot: %d\n", 
			       __FILE__,__LINE__, t, t->head, mem, cur_mem, max_mem, tot_mem );
    return t;
}

void textblock_add(textblock_t *t, const char *text, size_t size)
{
    size_t mem = size+sizeof(textdata_t);
    textdata_t *cur = t->tail;

    cur->size = size;
    if (size == 0)
	cur->data = NULL;
    else {
	cur->data = (char *)xmalloc(size);
	memcpy(cur->data, text, size);
    }
    cur_mem += mem;
    tot_mem += mem;
    max_mem = max(max_mem, cur_mem);
    if (DEBUG_TEXT(2)) fprintf(dbgout, "%s:%d  %p %p %3d add, cur: %d, max: %d, tot: %d\n", 
			       __FILE__,__LINE__, cur, cur->data, cur->size, cur_mem, max_mem, tot_mem );
    cur = cur->next = (textdata_t *) xcalloc(1, sizeof(textdata_t));
    t->tail = cur;
}

void textblock_free(textblock_t *t)
{
    size_t mem;
    textdata_t *cur, *nxt;
    for (cur = t->head; (nxt = cur->next) != NULL; cur = nxt) {
	mem = cur->size + sizeof(*cur);
	cur_mem -= mem;
	if (DEBUG_TEXT(2)) fprintf(dbgout, "%s:%d  %p %p %3d free, cur: %d, max: %d, tot: %d\n", 
				   __FILE__,__LINE__, cur, cur->data, cur->size, cur_mem, max_mem, tot_mem );
	xfree((void*)cur->data);
	xfree((void*)cur);
    }

    mem = sizeof(*t->head);
    cur_mem -= mem;

    if (DEBUG_TEXT(2)) fprintf(dbgout, "%s:%d  %p %p free, cur: %d, max: %d, tot: %d\n", 
			       __FILE__,__LINE__, t, t->head, cur_mem, max_mem, tot_mem );
    xfree(t);
    cur_mem -= sizeof(t->head) + sizeof(t);
    if (DEBUG_TEXT(1)) fprintf(dbgout, "cur: %d, max: %d, tot: %d\n", cur_mem, max_mem, tot_mem );
}
