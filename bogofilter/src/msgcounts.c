/* $Id$ */

/*****************************************************************************

NAME:
   msgcounts.c -- routines for setting & computing .MSG_COUNT values

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#include "common.h"

#include <ctype.h>
#include <stdlib.h>

#include "datastore.h"
#include "fgetsl.h"
#include "msgcounts.h"
#include "wordlists.h"

/* Globals */

char	msg_count_buff[MSG_COUNT_MAX_LEN];
int	msg_count_leng = MSG_COUNT_MAX_LEN;
char   *msg_count_text = msg_count_buff;

const char *msg_count_header = "\"" MSG_COUNT "\"";
size_t	    msg_count_header_len = 0;

uint	msgs_good = 0L;
uint	msgs_bad  = 0L;

static	bool	saved = false;

/* Function Definitions */

token_t  read_msg_count_line(void)
{
    bool msg_sep;

    if (!saved) {
	char *tmp = fgets(msg_count_buff, sizeof(msg_count_buff), fpin);
	if (tmp == NULL) {
	    msg_count_leng = 0;
	    return NONE;
	}
    }

    msg_count_leng = strlen(msg_count_buff);

    msg_sep = msg_count_buff[1] == '.' && 
	memcmp(msg_count_buff, msg_count_header, msg_count_header_len) == 0;

    if ( !saved && msg_sep ) {
	saved = true;
	return NONE;
    }
    else {
	saved = false;
	if (msg_sep)
	    return MSG_COUNT_LINE;
	else
	    return BOGO_LEX_LINE;
    }
}

bool msgcount_more(void)
{
    bool val = saved;
    saved = false;
    return val;
}

void init_msg_counts()
{
    msgs_good = 0L;
    msgs_bad  = 0L;
    msg_count_header_len= strlen(msg_count_header);
}

void set_msg_counts(char *s)
{
    msgs_bad = atoi(s);
    s = strchr(s, ' ') + 1;
    msgs_good = atoi(s);
}
