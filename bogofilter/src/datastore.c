/* $Id$ */

/*****************************************************************************

NAME:
datastore.c -- contains database independent components of data storage.

AUTHORS:
Gyepi Sam <gyepi@praxis-sw.com>   2002 - 2003
Matthias Andree <matthias.andree@gmx.de> 2003
David Relson <relson@osagesoftware.com>  2003

******************************************************************************/

#include "common.h"

#include <string.h>
#include <stdlib.h>
#include <limits.h>

#include "datastore.h"
#include "datastore_db.h"

#include "error.h"
#include "maint.h"
#include "swap.h"
#include "word.h"
#include "xmalloc.h"

#define struct_init(s) memset(&s, 0, sizeof(s))

YYYYMMDD today;			/* date as YYYYMMDD */

/* Function prototypes */

void ds_init(void);
void ds_cleanup(void);

/* Function definitions */

static
YYYYMMDD time_to_date(YYYYMMDD days)
{
    time_t t = time(NULL) - days * 86400;
    struct tm *tm = localtime( &t );
    YYYYMMDD date = ((((tm->tm_year + (YYYYMMDD)1900) * 100 + tm->tm_mon + 1) * 100) + tm->tm_mday);
    return date;
}

YYYYMMDD string_to_date(const char *s)
{
    YYYYMMDD date = (YYYYMMDD) atol(s);
    if (date < 20020801 && date != 0) {
	date = time_to_date(date);
    }
    return date;
}

void set_date(YYYYMMDD date)
{
    today = date;
}

void set_today(void)
{
    today = time_to_date(0);
}

static void convert_external_to_internal(dsh_t *dsh, dbv_t *ex_data, dsv_t *in_data)
{
    size_t i = 0;
    uint32_t *cv = ex_data->data;

    if (dsh->count == 1) {
	in_data->spamcount = !dsh->is_swapped ? cv[i++] : swap_32bit(cv[i++]);

	if (ex_data->leng <= i * sizeof(uint32_t))
	    in_data->goodcount = 0;
	else
	    in_data->goodcount = !dsh->is_swapped ? cv[i++] : swap_32bit(cv[i++]);

	if (ex_data->leng <= i * sizeof(uint32_t))
	    in_data->date = 0;
	else {
	    in_data->date = !dsh->is_swapped ? cv[i++] : swap_32bit(cv[i++]);
	}
    }
    else {
	in_data->count[dsh->index] = !dsh->is_swapped ? cv[i++] : swap_32bit(cv[i++]);

	if (ex_data->leng <= i * sizeof(uint32_t))
	    in_data->date = 0;
	else {
	    YYYYMMDD date = !dsh->is_swapped ? cv[i++] : swap_32bit(cv[i++]); 
	    in_data->date = max(in_data->date, date);
	}
    }

    return;
}

static void convert_internal_to_external(dsh_t *dsh, dsv_t *in_data, dbv_t *ex_data)
{
    size_t i = 0;
    uint32_t *cv = ex_data->data;

    /* Writing requires extra magic since the counts may need to be
    ** separated for output to different wordlists.
    */

    if (dsh->count == 1) {
	cv[i++] = !dsh->is_swapped ? in_data->spamcount : swap_32bit(in_data->spamcount);
	cv[i++] = !dsh->is_swapped ? in_data->goodcount : swap_32bit(in_data->goodcount);
    }
    else {
	cv[i++] = !dsh->is_swapped ? in_data->count[dsh->index] : swap_32bit(in_data->count[dsh->index]);
    }

    if (timestamp_tokens && in_data->date != 0)
	cv[i++] = !dsh->is_swapped ? in_data->date : swap_32bit(in_data->date);

    ex_data->leng = i * sizeof(cv[0]);

    return;
}

dsh_t *dsh_init(
    void *dbh,			/* database handle from db_open() */
    size_t count,		/* database count (1 or 2) */
    bool is_swapped)
{
    dsh_t *val = xmalloc(sizeof(*val));
    val->dbh = dbh;
    val->index = 0;
    val->count = count;
    val->is_swapped = is_swapped;
    return val;
}

void dsh_free(void *vhandle)
{
    dsh_t *dsh = vhandle;
    xfree(dsh);
    return;
}

void *ds_open(const char *db_file, size_t count, const char **names, dbmode_t open_mode)
{
    void *v = db_open(db_file, count, names, open_mode);
    return v;
}

void ds_close(/*@only@*/ void *vhandle, bool nosync  /** Normally false, if true, do not synchronize data. This should not be used in regular operation but only to ease the disk I/O load when the lock operation failed. */)
{
    dsh_t *dsh = vhandle;
    db_close(dsh->dbh, nosync);
    xfree(dsh);
}

void ds_flush(void *vhandle)
{
    dsh_t *dsh = vhandle;
    db_flush(dsh);
}

int ds_read(void *vhandle, const word_t *word, /*@out@*/ dsv_t *val)
{
    dsh_t *dsh = vhandle;
    bool found = false;
    dbv_t ex_key;
    dbv_t ex_data;
    uint32_t cv[3];

    struct_init(ex_key);
    struct_init(ex_data);

    ex_key.data = word->text;
    ex_key.leng = ex_key.size = word->leng;

    ex_data.data = cv;
    ex_data.leng = ex_data.size = sizeof(cv);

    memset(val, 0, sizeof(*val));

    for (dsh->index = 0; dsh->index < dsh->count; dsh->index++) {
	int ret = db_get_dbvalue(dsh, &ex_key, &ex_data);

	switch (ret) {
	case 0:
	    found = true;

	    convert_external_to_internal(dsh, &ex_data, val);

	    if (DEBUG_DATABASE(3)) {
		fprintf(dbgout, "ds_read: [%*s] -- %lu,%lu\n",
			(int)min(word->leng, INT_MAX), word->text,
			(unsigned long)val->spamcount,
			(unsigned long)val->goodcount);
	    }
	    break;

	case DS_NOTFOUND:
	    if (DEBUG_DATABASE(3)) {
		fprintf(dbgout, "ds_read: [%*s] not found\n", 
			(int)min(word->leng, INT_MAX), (char *) word->text);
	    }
	    break;
	    
	default:
	    fprintf(dbgout, "ret=%d, DS_NOTFOUND=%d\n", ret, DS_NOTFOUND);
	    print_error(__FILE__, __LINE__, "ds_read( '%*s' ), err: %d, %s", 
			(int)min(word->leng, INT_MAX), (char *) word->text, ret, db_str_err(ret));
	    exit(EX_ERROR);
	}
    }

    return found ? 0 : 1;
}

int ds_write(void *vhandle, const word_t *word, dsv_t *val)
{
    int ret = 0;
    dsh_t *dsh = vhandle;
    dbv_t ex_key;
    dbv_t ex_data;
    uint32_t cv[3];

    struct_init(ex_key);
    struct_init(ex_data);

    ex_key.data = word->text;
    ex_key.leng = word->leng;
    ex_key.size = word->leng;

    ex_data.data = cv;
    ex_data.size = sizeof(cv);

    if (timestamp_tokens && today != 0)
	val->date = today;

    for (dsh->index = 0; dsh->index < dsh->count; dsh->index++) {

	/* With two wordlists, it's necessary to check index and
	** run_type to avoid writing all tokens to both lists. */
	if (dsh->count == 2) {
	    bool ok;
	    /* if index is spamlist, but not writing to spamlist ... */
	    /* if index is goodlist, but not writing to goodlist ... */
	    ok = ((dsh->index == IX_SPAM && (run_type & (REG_SPAM | UNREG_SPAM))) ||
		  (dsh->index == IX_GOOD && (run_type & (REG_GOOD | UNREG_GOOD))));
	    if (!ok)
		continue;
	}

	convert_internal_to_external(dsh, val, &ex_data);

	ret = db_set_dbvalue(dsh, &ex_key, &ex_data);

	if (ret != 0)
	    break;

	if (DEBUG_DATABASE(3)) {
	    fprintf(dbgout, "ds_write: [%*s] -- %lu,%lu,%lu\n",
		    (int)min(word->leng, INT_MAX), word->text,
		    (unsigned long)val->spamcount,
		    (unsigned long)val->goodcount,
		    (unsigned long)val->date);
	}
    }

    return ret;		/* 0 if ok */
}

int ds_delete(void *vhandle, const word_t *word)
{
    dsh_t *dsh = vhandle;
    int ret;
    dbv_t ex_key;

    struct_init(ex_key);
    ex_key.data = word->text;
    ex_key.leng = word->leng;

    ret = db_delete(dsh, &ex_key);

    return ret;		/* 0 if ok */
}

typedef struct {
    ds_foreach_t *hook;
    dsh_t	 *dsh;
    void         *data;
} ds_userdata_t;

static int ds_hook(dbv_t *ex_key,
		   dbv_t *ex_data,
		   void *userdata)
{
    int ret;
    word_t w_key;
    dsv_t in_data;
    ds_userdata_t *ds_data = userdata;
    dsh_t *dsh = ds_data->dsh;

    w_key.text = ex_key->data;
    w_key.leng = ex_key->leng;

    memset(&in_data, 0, sizeof(in_data));
    convert_external_to_internal(dsh, ex_data, &in_data);

    ret = (*ds_data->hook)(&w_key, &in_data, ds_data->data);

    return ret;		/* 0 if ok */
}

int ds_foreach(void *vhandle, ds_foreach_t *hook, void *userdata)
{
    dsh_t *dsh = vhandle;
    int ret;
    ds_userdata_t ds_data;
    ds_data.hook = hook;
    ds_data.dsh  = dsh;
    ds_data.data = userdata;

    ret = db_foreach(dsh, ds_hook, &ds_data);

    return ret;
}

/* Wrapper for ds_foreach that opens and closes file */

int ds_oper(const char *path, dbmode_t open_mode, 
	    ds_foreach_t *hook, void *userdata)
{
    int  ret = 0;
    void *dsh = ds_open(CURDIR_S, 1, &path, open_mode);

    if (dsh == NULL) {
	fprintf(stderr, "Can't open file '%s'\n", path);
	exit(EX_ERROR);
    }

    ret = ds_foreach(dsh, hook, userdata);

    ds_close(dsh, false);

    return ret;
}

word_t  *msg_count_tok;

void ds_init()
{
    if (msg_count_tok == NULL) {
	msg_count_tok = word_new((const byte *)MSG_COUNT, strlen(MSG_COUNT));
    }
}

/* Cleanup storage allocation */
void ds_cleanup()
{
    xfree(msg_count_tok);
    msg_count_tok = NULL;
}

/*
  Get the number of messages associated with database.
*/
bool ds_get_msgcounts(void *vhandle, dsv_t *val)
{
    int rc;
    dsh_t *dsh = vhandle;
    ds_init();
    rc = ds_read(dsh, msg_count_tok, val);
    return rc == 0;
}

/*
 Set the number of messages associated with database.
*/
void ds_set_msgcounts(void *vhandle, dsv_t *val)
{
    dsh_t *dsh = vhandle;
    if (timestamp_tokens && val->date != 0)
	val->date = today;
    ds_write(dsh, msg_count_tok, val);
    return;
}

const char *ds_version_str(void)
{
    return db_version_str();
}
