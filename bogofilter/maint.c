/* $Id$ */

/*****************************************************************************

NAME:
   maint.c -- wordlist maintenance functions

AUTHOR:
   David Relson

******************************************************************************/

#include <config.h>
#include "system.h"		/* has time.h */

#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <db.h>

#include <assert.h>

#include "common.h"
#include "xmalloc.h"

#include "datastore.h"
#include "maint.h"

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

YYYYMMDD time_to_date(long days)
{
    time_t t = time(NULL) - days * 86400;
    struct tm *tm = localtime( &t );
    YYYYMMDD date = (((tm->tm_year + 1900) * 100 + tm->tm_mon + 1) * 100) + tm->tm_mday;
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
    if (thresh_count == 0)
	return true;
    else {
	bool ok = count > thresh_count;
	if (DEBUG_DATABASE(1)) fprintf(dbgout, "keep_count:  %d > %d -> %c\n", count, thresh_count, ok ? 't' : 'f' );
	return ok;
    }
}

/* Keep recent dates */
bool keep_date(YYYYMMDD date)
{
    if (thresh_date == 0 || date == 0 || date == today)
	return true;
    else {
	bool ok = thresh_date < date;
	if (DEBUG_DATABASE(1)) fprintf(dbgout, "keep_date:  %d < %d -> %c\n", (int) thresh_date, date, ok ? 't' : 'f' );
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

void do_replace_nonascii_characters(register byte *str, register size_t len) {
    assert(str != NULL);
    while(len--) {
	if (*str & 0x80) *str = '?';
	str++;
    }
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

    dbh = db_open(db_file, db_file, DB_WRITE, directory);
    if (dbh == NULL)
	return 2;

    rc = maintain_wordlist(dbh);

    return rc;
}

static int maintain_hook(char *key,  uint32_t keylen, 
			 char *data, uint32_t datalen, 
			 void *userdata /*@unused@*/)
{
    static uint32_t x_size = 40;
    static char *x;
    dbv_t val;
    memcpy(&val, data, datalen);

    (void)datalen;
    if (replace_nonascii_characters)
	do_replace_nonascii_characters((byte *)key,keylen);

    if (!keep_count(val.count) || !keep_date(val.date) || !keep_size(keylen)) {
	if (keylen + 1 > x_size) {
	    free(x);
	    x = NULL;
	    x_size = keylen + 1;
	}
	if (!x) x = xmalloc(x_size);

	memcpy(x, key, keylen);
	x[keylen] = '\0';

	db_delete(userdata, x);

	if (DEBUG_DATABASE(0)) fprintf(dbgout, "deleting %s\n", x);
    }
    return 0;
}

int maintain_wordlist(void *vhandle)
{
    void *dbh = vhandle;
    return db_foreach(dbh, maintain_hook, dbh);
}
