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

#include "common.h"

#include "datastore.h"
#include "datastore_db.h"
#include "maint.h"

YYYYMMDD today;			/* date as YYYYMMDD */
int	 thresh_count = 0;
YYYYMMDD thresh_date  = 0;
size_t	 size_min = 0;
size_t	 size_max = 0;
bool     datestamp_tokens = true;
bool	 replace_nonascii_characters = false;

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
bool keep_count(int count)
{
    if (thresh_count == 0)
	return true;
    else {
	bool ok = count > thresh_count;
	if (DEBUG_DATABASE(1)) fprintf(stderr, "keep_count:  %d > %d -> %c\n", count, thresh_count, ok ? 't' : 'f' );
	return ok;
    }
}

/* Keep recent dates */
bool keep_date(int date)
{
    if (thresh_date == 0 || date == 0 || date == today)
	return true;
    else {
	bool ok = thresh_date < date;
	if (DEBUG_DATABASE(1)) fprintf(stderr, "keep_date:  %d < %d -> %c\n", (int) thresh_date, date, ok ? 't' : 'f' );
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
	if (DEBUG_DATABASE(1)) fprintf(stderr, "keep_size:  %d <= %d <= %d -> %c\n", size_min, size, size_max, ok ? 't' : 'f' );
	return ok;
    }
}

void do_replace_nonascii_characters(byte *str)
{
    byte ch;
    while ((ch=*str) != '\0')
    {
	if ( ch & 0x80)
	    *str = '?';
	str += 1;
    }
}

void maintain_wordlists(void)
{
    wordlist_t *list;

    good_list.active = spam_list.active = true;

    db_lock_writer_list(word_lists);

    for (list = word_lists; list != NULL; list = list->next) {
	maintain_wordlist(list->dbh);
	list = list->next;
    }

    db_lock_release_list(word_lists);
}

int maintain_wordlist_file(const char *db_file)
{
    int rc;
    dbh_t *dbh;

    dbh = db_open(db_file, db_file, DB_WRITE);
    if (dbh == NULL)
	return 2;

    db_lock_writer(dbh);

    rc = maintain_wordlist(dbh);

    db_lock_release(dbh);

    return rc;
}

int maintain_wordlist(void *vhandle)
{
    dbh_t *dbh = vhandle;

    DBC dbc;
    DBC *dbcp;
    DBT db_key, db_data;

    int ret;
    int rv = 0;
    dbv_t *val;

    dbcp = &dbc;

    memset(&db_key, 0, sizeof(DBT));
    memset(&db_data, 0, sizeof(DBT));

    if ((ret = dbh->dbp->cursor(dbh->dbp, NULL, &dbcp, 0) != 0)) {
	dbh->dbp->err(dbh->dbp, ret, "%s (cursor): %s", progname, dbh->filename);
	return (2);
    }

    for (;;) {
	ret = dbcp->c_get(dbcp, &db_key, &db_data, DB_NEXT);
	if (ret == 0) {
	    val = (dbv_t *)db_data.data;
	    if (replace_nonascii_characters)
		do_replace_nonascii_characters((byte *)db_key.data);

	    if (! keep_count(val->count) || ! keep_date(val->date) || ! keep_size(db_key.size)) {
		char *token = (char *) db_key.data;
		int rc1, rc2;
		token[db_key.size] = '\0';
		rc1 = dbcp->c_del(dbcp, 0);
		rc2 = dbh->dbp->del(dbh->dbp, NULL, &db_key, 0);

		if (DEBUG_DATABASE(0)) fprintf(stderr, "deleting %s --> %d, %d\n", (char *)db_key.data, rc1, rc2);
	    }
	}
	else if (ret == DB_NOTFOUND) {
	    break;
	}
	else {
	    dbh->dbp->err(dbh->dbp, ret, "%s (c_get)", progname);
	    rv = 2;
	    break;
	}
    }

    return rv;
}
