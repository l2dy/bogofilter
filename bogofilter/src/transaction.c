/* $Id$ */

/*****************************************************************************

NAME:
transaction.c -- implements transactions on datastore

AUTHORS:
Stefan Bellon <sbellon@sbellon.de>       2003

******************************************************************************/

#include "common.h"

#include <string.h>

#include "maint.h"
#include "transaction.h"
#include "xmalloc.h"

typedef enum ta_kind {
    TA_DELETE,
    TA_WRITE
} ta_kind_t;

typedef struct ta_iter {
    ta_kind_t kind;
    void *vhandle;
    word_t *word;
    dsv_t *dsvval;
    struct ta_iter *next;
    struct ta_iter *prev;
} ta_iter_t;

struct ta_type {
    ta_iter_t *head;
    ta_iter_t *last;
};

ta_t *ta_init(void)
{
    ta_t *ta = xmalloc(sizeof(*ta));
    ta->head = NULL;
    ta->last = NULL;
    return ta;
}

static int ta_flush(ta_t *ta, bool write)
{
    int ret = TA_OK;
    ta_iter_t *tmp, *iter = ta->head;
    
    while (iter) {
        if (write) {
            switch (iter->kind) {
            case TA_DELETE:
                ret |= ds_delete(iter->vhandle, iter->word);
                break;
            case TA_WRITE:
		set_date(iter->dsvval->date);
                ret |= ds_write(iter->vhandle, iter->word, iter->dsvval);
                break;
            }
        }
        if (iter->word)
            word_free(iter->word);
        if (iter->dsvval)
            xfree(iter->dsvval);
        tmp = iter;
        iter = iter->next;
        xfree(tmp);
    }
    xfree(ta);

    return ret;
}

int ta_commit(ta_t *ta)
{
    if (ta == NULL)
        return TA_ERR;
    return ta_flush(ta, true);
}

int ta_rollback(ta_t *ta)
{
    if (ta == NULL)
        return TA_ERR;

    return ta_flush(ta, false);
}

static void ta_add(ta_t *ta, ta_kind_t ta_kind, void *vhandle,
                   const word_t *word, dsv_t *dsvval)
{
    void *ta_vhandle = vhandle;
    word_t *ta_word = NULL;
    dsv_t *ta_dsvval = NULL;
    ta_iter_t *item = NULL;

    if (word)
        ta_word = word_dup(word);
    
    if (dsvval) {
        ta_dsvval = xmalloc(sizeof(*ta_dsvval));
	memcpy(ta_dsvval, dsvval, sizeof(*ta_dsvval));
    }
    
    item = xmalloc(sizeof(*item));
    item->kind = ta_kind;
    item->vhandle = ta_vhandle;
    item->word = ta_word;
    item->dsvval = ta_dsvval;
    item->next = NULL;
    item->prev = ta->last;
    
    if (ta->head == NULL)
        ta->head = item;
    if (ta->last != NULL)
        ta->last->next = item;
    ta->last = item;
}

int ta_delete(ta_t *ta, void *vhandle, const word_t *word)
{
    if (ta == NULL)
        return TA_ERR;
    
    ta_add(ta, TA_DELETE, vhandle, word, NULL);

    return TA_OK;
}

int ta_write(ta_t *ta, void *vhandle, const word_t *word, dsv_t *val)
{
    if (ta == NULL)
        return TA_ERR;
    
    ta_add(ta, TA_WRITE, vhandle, word, val);

    return TA_OK;
}

int ta_read(ta_t *ta, void *vhandle, const word_t *word, /*@out@*/ dsv_t *val)
{
    ta_iter_t *iter;

    if (ta == NULL)
        return TA_ERR;
    
    memset(val, 0, sizeof(*val));

    /* search transaction list from end to start */
    iter = ta->last;
    while (iter) {
        if (word_cmp(word, iter->word) == 0) {
            switch (iter->kind) {
            case TA_DELETE:
                return TA_ERR; /* token deleted, so not found */
            case TA_WRITE:
                if (iter->dsvval) {
		    memcpy(val, iter->dsvval, sizeof(*iter->dsvval));
                    return TA_OK;  /* token found */
                }
                else
                    return TA_ERR; /* value should be present! ERROR! */
            }
        }
        iter = iter->prev;
    }
    
    /* token not found in our transaction list, so ask the database backend */
    return ds_read(vhandle, word, val);
}
