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
#include "rand_sleep.h"
#include "swap.h"
#include "word.h"
#include "xmalloc.h"

#define struct_init(s) memset(&s, 0, sizeof(s))

YYYYMMDD today;			/* date as YYYYMMDD */

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

    in_data->spamcount = !dsh->is_swapped ? cv[i++] : swap_32bit(cv[i++]);

    if (ex_data->leng <= i * sizeof(uint32_t))
	in_data->goodcount = 0;
    else
	in_data->goodcount = !dsh->is_swapped ? cv[i++] : swap_32bit(cv[i++]);

    if (ex_data->leng <= i * sizeof(uint32_t))
	in_data->date = 0;
    else
	in_data->date = !dsh->is_swapped ? cv[i++] : swap_32bit(cv[i++]);

    return;
}

static void convert_internal_to_external(dsh_t *dsh, dsv_t *in_data, dbv_t *ex_data)
{
    size_t i = 0;
    uint32_t *cv = ex_data->data;

    /* Writing requires extra magic since the counts may need to be
    ** separated for output to different wordlists.
    */

    cv[i++] = !dsh->is_swapped ? in_data->spamcount : swap_32bit(in_data->spamcount);
    cv[i++] = !dsh->is_swapped ? in_data->goodcount : swap_32bit(in_data->goodcount);

    if (timestamp_tokens && in_data->date != 0)
	cv[i++] = !dsh->is_swapped ? in_data->date : swap_32bit(in_data->date);

    ex_data->leng = i * sizeof(cv[0]);

    return;
}

dsh_t *dsh_init(
    void *dbh)			/* database handle from db_open() */
{
    dsh_t *val = xmalloc(sizeof(*val));
    val->dbh = dbh;
    val->is_swapped = db_is_swapped(dbh);
    return val;
}

void dsh_free(void *vhandle)
{
    dsh_t *dsh = vhandle;
    xfree(dsh);
    return;
}

void *ds_open(void *dbe, const char *path, const char *name, dbmode_t open_mode)
{
    dsh_t *dsh;
    void *v;

    v = db_open(dbe, path, name, open_mode); /* FIXME */

    if (!v)
	return NULL;

    dsh = dsh_init(v);

    if (db_created(v) && ! (open_mode & DS_LOAD)) {
	ds_set_wordlist_version(dsh, NULL);
    }
    return dsh;
}

void ds_close(/*@only@*/ void *vhandle)
{
    dsh_t *dsh = vhandle;
    db_close(dsh->dbh);
    xfree(dsh);
}

void ds_flush(void *vhandle)
{
    dsh_t *dsh = vhandle;
    db_flush(dsh->dbh);
}

int ds_read(void *vhandle, const word_t *word, /*@out@*/ dsv_t *val)
{
    int ret;
    bool found = false;
    dsh_t *dsh = vhandle;
    dbv_t ex_key;
    dbv_t ex_data;
    uint32_t cv[3];

    struct_init(ex_key);
    struct_init(ex_data);

    ex_key.data = word->text;
    ex_key.leng = word->leng;

    memset(val, 0, sizeof(*val));

    /* init ex_data inside loop since first db_get_value()
    ** call can change it and cause the second call to fail.
    */
    ex_data.data = cv;
    ex_data.leng = sizeof(cv);

    ret = db_get_dbvalue(dsh->dbh, &ex_key, &ex_data);

    switch (ret) {
    case 0:
	found = true;

	convert_external_to_internal(dsh, &ex_data, val);

	if (DEBUG_DATABASE(3)) {
	    fprintf(dbgout, "ds_read: [%.*s] -- %lu,%lu\n",
		    CLAMP_INT_MAX(word->leng), (const char *)word->text,
		    (unsigned long)val->spamcount,
		    (unsigned long)val->goodcount);
	}
	return 0;

    case DS_NOTFOUND:
	if (DEBUG_DATABASE(3)) {
	    fprintf(dbgout, "ds_read: [%.*s] not found\n", 
		    CLAMP_INT_MAX(word->leng), (char *) word->text);
	}
	return 1;

    case DS_ABORT_RETRY:
	if (DEBUG_DATABASE(1)) {
	    print_error(__FILE__, __LINE__, "ds_read('%.*s') was aborted to recover from a deadlock.",
		    CLAMP_INT_MAX(word->leng), (char *) word->text);
	}
	break;

    default:
	fprintf(dbgout, "ret=%d, DS_NOTFOUND=%d\n", ret, DS_NOTFOUND);
	print_error(__FILE__, __LINE__, "ds_read( '%.*s' ), err: %d, %s", 
		    CLAMP_INT_MAX(word->leng), (char *) word->text, ret, db_str_err(ret));
	exit(EX_ERROR);
    }

    return ret;
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

    ex_data.data = cv;
    ex_data.leng = sizeof(cv);

    if (timestamp_tokens && today != 0)
	val->date = today;

    convert_internal_to_external(dsh, val, &ex_data);

    ret = db_set_dbvalue(dsh->dbh, &ex_key, &ex_data);

    if (DEBUG_DATABASE(3)) {
	fprintf(dbgout, "ds_write: [%.*s] -- %lu,%lu,%lu\n",
		CLAMP_INT_MAX(word->leng), (const char *)word->text,
		(unsigned long)val->spamcount,
		(unsigned long)val->goodcount,
		(unsigned long)val->date);
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

    ret = db_delete(dsh->dbh, &ex_key);

    return ret;		/* 0 if ok */
}

int ds_txn_begin(void *vhandle) {
    return dbe_txn_begin(vhandle);
}

int ds_txn_abort(void *vhandle) {
    return dbe_txn_abort(vhandle);
}

int ds_txn_commit(void *vhandle) {
    return dbe_txn_commit(vhandle);
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

    ret = db_foreach(dsh->dbh, ds_hook, &ds_data);

    return ret;
}

/* Wrapper for ds_foreach that opens and closes file */

int ds_oper(void *env, const char *path, dbmode_t open_mode, 
	    ds_foreach_t *hook, void *userdata)
{
    int  ret = 0;
    void *dsh;

    dsh = ds_open(env, CURDIR_S, path, open_mode);

    if (dsh == NULL) {
	fprintf(stderr, "Can't open file '%s'\n", path);
	exit(EX_ERROR);
    }

    if (DST_OK == ds_txn_begin(env)) {
	ret = ds_foreach(dsh, hook, userdata);
	if (ret) { ds_txn_abort(env); }
	else
	    if (ds_txn_commit(env) != DST_OK)
		ret = -1;
    }

    ds_close(dsh);

    return ret;
}

static word_t  *msg_count_tok;
static word_t  *wordlist_version_tok;

void *ds_init(const char *directory)
{
    void *dbe = dbe_init(directory);
    if (msg_count_tok == NULL) {
	msg_count_tok = word_new((const byte *)MSG_COUNT, strlen(MSG_COUNT));
    }
    if (wordlist_version_tok == NULL) {
	wordlist_version_tok = word_new((const byte *)WORDLIST_VERSION,
		strlen(WORDLIST_VERSION));
    }
    return dbe;
}

/* Cleanup storage allocation */
void ds_cleanup(void *dbe)
{
    dbe_cleanup(dbe);
    xfree(msg_count_tok);
    xfree(wordlist_version_tok);
    msg_count_tok = NULL;
    wordlist_version_tok = NULL;
}

/*
  Get the number of messages associated with database.
*/
int ds_get_msgcounts(void *vhandle, dsv_t *val)
{
    dsh_t *dsh = vhandle;

    return ds_read(dsh, msg_count_tok, val);
}

/*
 Set the number of messages associated with database.
*/
int  ds_set_msgcounts(void *vhandle, dsv_t *val)
{
    dsh_t *dsh = vhandle;

    val->date = today;

    return ds_write(dsh, msg_count_tok, val);
}

void *ds_get_dbenv(void *vhandle)
{
    dsh_t *dsh = vhandle;
    return db_get_env(dsh->dbh);
}

/*
  Get the wordlist version associated with database.
*/
int ds_get_wordlist_version(void *vhandle, dsv_t *val)
{
    dsh_t *dsh = vhandle;

    return ds_read(dsh, wordlist_version_tok, val);
}

/*
 Set the wordlist version associated with database.
*/
int ds_set_wordlist_version(void *vhandle, dsv_t *val)
{
    dsh_t *dsh = vhandle;
    dsv_t  tmp;

    if (val == NULL)
    {
	val = &tmp;
	val->count[0] = CURRENT_VERSION;
	val->count[1] = 0;
    }

    val->date = today;

    return ds_write(dsh, wordlist_version_tok, val);
}

const char *ds_version_str(void)
{
    return db_version_str();
}

int ds_recover(const char *directory, bool catastrophic)
{
    return dbe_recover(directory, catastrophic, true);
}

int ds_remove(const char *directory) {
    return dbe_remove(directory);
}

int ds_purgelogs(const char *directory) {
    return dbe_purgelogs(directory);
}
