/* $Id$ */

/*****************************************************************************

NAME:
   maint.c -- wordlist maintenance functions

AUTHOR:
   David Relson

******************************************************************************/

#include <assert.h>
#include <db.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include <config.h>
#include "common.h"

#include "datastore.h"
#include "error.h"
#include "maint.h"
#include "xmalloc.h"
#include "xstrdup.h"

YYYYMMDD today;			/* date as YYYYMMDD */
uint32_t thresh_count = 0;
YYYYMMDD thresh_date  = 0;
size_t	 size_min = 0;
size_t	 size_max = 0;
bool     datestamp_tokens = true;

/* Function Prototypes */
static YYYYMMDD time_to_date(long days);

static int maintain_wordlist(void *vhandle);

/* Function Definitions */

static
YYYYMMDD time_to_date(long days)
{
    time_t t = time(NULL) - days * 86400;
    struct tm *tm = localtime( &t );
    YYYYMMDD date = ((((tm->tm_year + (YYYYMMDD)1900) * 100 + tm->tm_mon + 1) * 100) + tm->tm_mday);
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

YYYYMMDD string_to_date(const char *s)
{
    YYYYMMDD date = atol(s);
    if (date < 20020801 && date != 0) {
	date = time_to_date(date);
    }
    return date;
}

/* Keep high counts */
bool keep_count(uint32_t count)
{
    bool ok = count > thresh_count;
    if (count > 0 && DEBUG_DATABASE(1))
	fprintf(dbgout, "keep_count:  %lu > %lu -> %c\n",
		(unsigned long)count, (unsigned long)thresh_count,
		ok ? 't' : 'f' );
    return ok;
}

/* Keep recent dates */
bool keep_date(YYYYMMDD date)
{
    if (thresh_date == 0 || date == 0 || date == today)
	return true;
    else {
	bool ok = thresh_date < date;
	if (DEBUG_DATABASE(1))
	    fprintf(dbgout, "keep_date: %ld < %ld -> %c\n",
		    (long)thresh_date, (long)date, ok ? 't' : 'f' );
	return ok;
    }
}

/* Keep sizes within bounds */
bool keep_size(size_t size)
{
    if (size_min == 0 || size_max == 0)
	return true;
    else {
	bool ok = (size_min <= size) && (size <= size_max);
	if (DEBUG_DATABASE(1)) fprintf(dbgout, "keep_size:  %lu <= %lu <= %lu -> %c\n", (unsigned long)size_min, (unsigned long)size, (unsigned long)size_max, ok ? 't' : 'f' );
	return ok;
    }
}

bool do_replace_nonascii_characters(register byte *str, register size_t len)
{
    bool change = false;
    assert(str != NULL);
    while(len--) {
	if (*str & 0x80) {
	    *str = '?';
	    change = true;
	}
	str++;
    }
    return change;
}

void maintain_wordlists(void)
{
    wordlist_t *list;

    set_list_active_status(true);

    for (list = word_lists; list != NULL; list = list->next) {
	maintain_wordlist(list->dbh);
	list = list->next;
    }
}

int maintain_wordlist_file(const char *db_file)
{
    int rc;
    void *dbh;

    dbh = db_open(".", 1, &db_file, DB_WRITE);
    if (dbh == NULL)
	return 2;

    rc = maintain_wordlist(dbh);

    db_close(dbh, false);

    return rc;
}

static int maintain_hook(word_t *key, word_t *data,
			 /*@unused@*/ void *userdata)
{
    word_t w;
    dbv_t val;

    if (data->leng > sizeof(val)) {
	print_error(__FILE__, __LINE__, "Invalid database value.\n");
	exit(2);
    }

    w.leng = key->leng;
    w.text = key->text;

    memcpy(&val, data->text, data->leng);

    if (strncmp(key->text, ".MSG_COUNT", key->leng) == 0)
	return 0;

    if ((!keep_count(val.spamcount) && !keep_count(val.goodcount)) || 
	!keep_date(val.date) || !keep_size(key->leng)) {
	db_delete(userdata, key);
	if (DEBUG_DATABASE(0)) {
	    fputs("deleting ", dbgout);
	    word_puts(&w, 0, dbgout);
	    fputc('\n', dbgout);
	}
    }
    else {
	if (replace_nonascii_characters)
	{
	    byte *tmp = (byte *)xmalloc(key->leng + 1);
	    memcpy(tmp, key->text, key->leng);
	    tmp[key->leng] = '\0';
	    if (do_replace_nonascii_characters(tmp, key->leng))
	    {
		db_delete(userdata, key);
		w.text = tmp;
		w.leng = key->leng;
		db_updvalues(userdata, &w, &val);
	    }
	    xfree(tmp);
	}
    }
    return 0;
}

int maintain_wordlist(void *vhandle)
{
    void *dbh = vhandle;
    return db_foreach(dbh, maintain_hook, dbh);
}
