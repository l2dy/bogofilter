/* $Id$ */

/*****************************************************************************

NAME:
datastore.c -- contains database independent components of data storage.

AUTHORS:
Gyepi Sam <gyepi@praxis-sw.com>   2002 - 2003
Matthias Andree <matthias.andree@gmx.de> 2003

******************************************************************************/

#include "datastore.h"
#include "xmalloc.h"
#include  "maint.h"

word_t  *msg_count_tok;

/* Cleanup storage allocation */
void db_cleanup()
{
    xfree(msg_count_tok);
    msg_count_tok = NULL;
}


/*
    Retrieve numeric value associated with word.
    Returns: value if the the word is found in database,
    0 if the word is not found.
    Notes: Will call exit if an error occurs.
*/
bool db_getvalues(void *vhandle, const word_t *word, dbv_t *val)
{
    int ret;

    ret = db_get_dbvalue(vhandle, word, val);

    if (ret == 0) {
	if ((int32_t)val->spamcount < (int32_t)0)
	    val->spamcount = 0;
	if ((int32_t)val->goodcount < (int32_t)0)
	    val->goodcount = 0;
	return true;
    } else {
	memset(val, 0, sizeof(*val));
	return false;
    }
}


/*
Store VALUE in database, using WORD as database key
Notes: Calls exit if an error occurs.
*/
void db_setvalues(void *vhandle, const word_t *word, dbv_t *val)
{
    val->date = today;		/* date in form YYYYMMDD */
    db_set_dbvalue(vhandle, word, val);
}


/*
Update the VALUE in database, using WORD as database key.
Adds COUNT to existing count.
Sets date to newer of TODAY and date in database.
*/
void db_updvalues(void *vhandle, const word_t *word, const dbv_t *updval)
{
    dbv_t val;
    int ret = db_get_dbvalue(vhandle, word, &val);
    if (ret != 0) {
	val.spamcount = updval->spamcount;
	val.goodcount = updval->goodcount;
	val.date      = updval->date;			/* date in form YYYYMMDD */
    }
    else {
	val.spamcount += updval->spamcount;
	val.goodcount += updval->goodcount;
	val.date       = max(val.date, updval->date);	/* date in form YYYYMMDD */
    }
    db_set_dbvalue(vhandle, word, &val);
}


/*
  Increment count associated with WORD, by VALUE.
 */
void db_increment(void *vhandle, const word_t *word, dbv_t *val)
{
    dbv_t cur;

    db_getvalues(vhandle, word, &cur);

    cur.spamcount = UINT32_MAX - cur.spamcount < val->spamcount ? UINT32_MAX : cur.spamcount + val->spamcount;
    cur.goodcount = UINT32_MAX - cur.goodcount < val->goodcount ? UINT32_MAX : cur.goodcount + val->goodcount;

    db_setvalues(vhandle, word, &cur);

    return;
}


/*
  Decrement count associated with WORD by VALUE,
  if WORD exists in the database.
*/
void db_decrement(void *vhandle, const word_t *word, dbv_t *val)
{
    dbv_t cur;

    db_getvalues(vhandle, word, &cur);

    cur.spamcount = cur.spamcount < val->spamcount ? 0 : cur.spamcount - val->spamcount;
    cur.goodcount = cur.goodcount < val->goodcount ? 0 : cur.goodcount - val->goodcount;
    db_setvalues(vhandle, word, &cur);

    return;
}


/*
  Get the number of messages associated with database.
*/
void db_get_msgcounts(void *vhandle, dbv_t *val)
{
    if (msg_count_tok == NULL)
	msg_count_tok = word_new(MSG_COUNT_TOK, strlen((const char *)MSG_COUNT_TOK));

    db_getvalues(vhandle, msg_count_tok, val);

    if (DEBUG_DATABASE(2)) {
	dbh_print_names(vhandle, "db_get_msgcounts");
	fprintf(dbgout, " ->  %lu,%lu\n",
		(unsigned long)val->spamcount,
		(unsigned long)val->goodcount);
    }

    return;
}


/*
 Set the number of messages associated with database.
*/
void db_set_msgcounts(void *vhandle, dbv_t *val)
{
    db_setvalues(vhandle, msg_count_tok, val);

    if (DEBUG_DATABASE(2)) {
	dbh_print_names(vhandle, "db_get_msgcounts");
	fprintf(dbgout, " ->  %lu,%lu\n",
		(unsigned long)val->spamcount,
		(unsigned long)val->goodcount);
    }
}


/* implements locking. */
int db_lock(int fd, int cmd, short int type)
{
    struct flock lock;

    lock.l_type = type;
    lock.l_start = 0;
    lock.l_whence = (short int)SEEK_SET;
    lock.l_len = 0;
    return (fcntl(fd, cmd, &lock));
}
