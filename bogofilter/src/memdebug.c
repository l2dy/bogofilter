/* $Id$ */

/*
* NAME:
*    memdebug.c -- memory usage debugging layer for malloc(), free(), etc.
*
* AUTHOR:
*    David Relson <relson@osagesoftware.com>
*
* NOTE:
*    These routines are useful, though not especially polished.
*
*    Capabilities:
*
*	- Count calls to malloc(), free(), realloc(), calloc().
*	- For each such call, track current, maximum, and total bytes allocated.
*	- Display summary statistics.
*	- Display size and address of each block as allocated or freed to locate the unfreed blocks.
*	- Trap routine than can be called by allocation index or allocation size.
*	- Program exit when current allocation is "too much".
*/

#include "common.h"

#include <stdlib.h>

#define	NO_MEMDEBUG_MACROS
#include "memdebug.h"
#include "xmalloc.h"

int  memtrace= 0 * (M_MALLOC+M_FREE);

uint32_t dbg_trap_index= 0;
uint32_t dbg_size      = 0;
uint32_t dbg_index_min = 0;
uint32_t dbg_index_max = 0;
uint32_t dbg_size_trap = 0;	/* 100000 */

#define	MB 1000000
#define	GB 1000*MB
uint32_t dbg_too_much  = 0;	/* GB */

uint32_t dbg_size_delt = 0;	/* MB */
uint32_t dbg_delt_save = 0;

const uint32_t md_tag = 0xABCD55AA;

uint32_t cnt_malloc = 0;
uint32_t cnt_free   = 0;
uint32_t cnt_realloc= 0;
uint32_t cnt_calloc = 0;
uint32_t cur_malloc = 0;
uint32_t max_malloc = 0;
uint32_t tot_malloc = 0;

void md_trap(const char *why);
void md_trap(const char *why) { (void)why; }

typedef struct memheader {
    size_t	size;
    uint32_t	indx;
    uint32_t	tag;
} mh_t;

void mh_disp(const char *s, mh_t *p);
void mh_disp(const char *s, mh_t *p)
{
    if (dbg_index_min != 0 && p->indx < dbg_index_min)
	return;
    if (dbg_index_max != 0 && p->indx > dbg_index_max)
	return;

/*    if (p->size == dbg_size) */
	fprintf(dbgout, "::%3d  %08lX  %s  %lu\n", p->indx, (ulong) (p+1), s, (ulong) p->size);
}

void *
md_malloc(size_t size)
{
    void *x;
    mh_t *mh = NULL;

    ++cnt_malloc;
    cur_malloc += size;
    
    if (dbg_size_delt != 0 &&
	max_malloc - dbg_delt_save > dbg_size_delt) {
	dbg_delt_save = max_malloc / dbg_size_delt * dbg_size_delt;
	fprintf(dbgout, ":: max_malloc = %12lu\n", (ulong) max_malloc);
    }

    max_malloc = max(max_malloc, cur_malloc);
    tot_malloc += size;
    size += sizeof(mh_t);		/* Include size storage */

    if (dbg_size_trap != 0 && size > dbg_size_trap)
	md_trap("dbg_size_trap");

    if (dbg_too_much != 0 && max_malloc > dbg_too_much) {
	fprintf(stderr, "max_malloc = %12lu, tot_malloc = %12lu\n", (ulong) max_malloc, (ulong) tot_malloc);
	exit(EX_ERROR);
    }

    x = malloc(size);

    mh = (mh_t *) x;
    mh->size = size - sizeof(mh_t);
    mh->indx = cnt_malloc;
    mh->tag  = md_tag;

    if (memtrace & M_MALLOC)
	mh_disp( "a", mh );
    if (dbg_trap_index != 0 && mh->indx == dbg_trap_index)
	md_trap("dbg_index");

    x = (void *) (mh+1);

    return x;
}

void
md_free(void *ptr)
{
    mh_t *mh = NULL;

    if (!ptr)
	return;

    mh = ((mh_t *) ptr) - 1;

    ++cnt_free;
    if (memtrace & M_FREE)
	mh_disp( "f", mh );

    if (mh->tag != md_tag)
	md_trap("md_tag");
    if (dbg_trap_index != 0 && mh->indx == dbg_trap_index)
	md_trap("dbg_trap_index");
    if (mh->size > cur_malloc || 
	(dbg_size_trap != 0 && mh->size > dbg_size_trap))
	md_trap("dbg_size_trap");
    cur_malloc -= mh->size;

    mh->tag = -1;
    ptr = (void *) mh;

    free(ptr);
}

void memdisplay(const char *file, int lineno)
{
    const char *pfx  = "";

    if (file != NULL) {
	pfx = "\t";
	fprintf(dbgout, "%s:%d:\n", file, lineno);
    }

    fprintf(dbgout, "%smalloc:  cur = %lu, max = %lu, tot = %lu\n", pfx,
	    (ulong) cur_malloc, (ulong) max_malloc, (ulong) tot_malloc );
    fprintf(dbgout, "%scounts:  malloc: %lu, calloc: %lu, realloc: %lu, free: %lu\n", pfx,
	    (ulong) cnt_malloc, (ulong) cnt_realloc, (ulong) cnt_calloc, (ulong) cnt_free);
    if (cnt_malloc == cnt_free)
	fprintf(dbgout, "%s         none active.\n", pfx);
    else
	fprintf(dbgout, "%s         active: %lu, average: %lu\n", pfx, 
		(ulong) cnt_malloc - cnt_free, (ulong) cur_malloc/(cnt_malloc - cnt_free));
}

void
*md_calloc(size_t nmemb, size_t size)
{
    void *x;
    mh_t *mh;

    size = size * nmemb;
    nmemb = 1;
    cur_malloc += size;
    max_malloc = max(max_malloc, cur_malloc);
    tot_malloc += size;
    size += sizeof(mh_t);		/* Include size storage */
    ++cnt_malloc;

    if (dbg_too_much != 0 && max_malloc > dbg_too_much) {
	fprintf(stderr, "max_malloc = %12lu, tot_malloc = %12lu\n", 
		(ulong) max_malloc, (ulong) tot_malloc);
	exit(EX_ERROR);
    }

    x = calloc(nmemb, size);

    mh = (mh_t *) x;
    mh->size = size - sizeof(mh_t);
    mh->indx = cnt_malloc;
    mh->tag  = md_tag;

    if (memtrace & M_MALLOC)
	mh_disp( "c", mh );
    if (dbg_trap_index != 0 && mh->indx == dbg_trap_index)
	md_trap("dbg_trap_index");

    x = (void *) (mh+1);

    return x;
}

void
*md_realloc(void *ptr, size_t size)
{
    void *x;
    size_t *s = (size_t *) ptr;
    size_t oldsize = *--s;

    cur_malloc -= oldsize;
    ptr = (void *) s;
    cur_malloc += size - oldsize;
    max_malloc = max(max_malloc, cur_malloc);
    tot_malloc += size - oldsize;
    size += sizeof(size_t);
    ++cnt_realloc;

    if (dbg_too_much != 0 && max_malloc > dbg_too_much) {
	fprintf(stderr, "max_malloc = %12lu, tot_malloc = %12lu\n", 
		(ulong) max_malloc, (ulong) tot_malloc);
	exit(EX_ERROR);
    }

    x = realloc(ptr, size);

    s = (size_t *) x;
    *s++ = size;
    x = (void *) s;

    return x;
}
