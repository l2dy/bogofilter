/* $Id$ */

/*****************************************************************************

NAME:
   msgcounts.c -- routines for setting & computing .MSG_COUNT values

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include <config.h>
#include "common.h"

#include "fgetsl.h"
#include "msgcounts.h"
#include "wordlists.h"

/* Globals */

bool	msg_count_file = false;
char	msg_count_chars[MSG_COUNT_MAX_LEN];
size_t	msg_count_leng = MSG_COUNT_MAX_LEN;
char   *msg_count_text = msg_count_chars;

const char *msg_count_header = "\".MSG_COUNT\"";

long	msgs_good = 0L;
long	msgs_bad  = 0L;

/* Function Definitions */

token_t  msg_count_lex(void)
{
    char *tmp = fgets(msg_count_chars, sizeof(msg_count_chars), fpin);
    if (tmp == NULL) {
	msg_count_leng = 0;
	return NONE;
    }
    else {
	msg_count_leng = strlen(msg_count_text);

	while (msg_count_leng > 0 && isspace(msg_count_text[msg_count_leng-1])) {
	    msg_count_leng -= 1;
	    msg_count_text[msg_count_leng] = '\0';
	}
	if ( memcmp(msg_count_text, msg_count_header, strlen(msg_count_header)) == 0 )
	    return MSG_COUNT_LINE;
	else
	    return BOGO_LEX_LINE;
    }
}

void init_msg_counts()
{
    msgs_good = 0L;
    msgs_bad  = 0L;
}

void compute_msg_counts()
{
    wordlist_t* list;

    for(list=word_lists; list != NULL; list=list->next)
    {
	if (list->bad)
	    msgs_bad += list->msgcount;
	else
	    msgs_good += list->msgcount;
    }
}

void set_msg_counts(char *s)
{
    msgs_bad = atoi(s);
    s = index(s, ' ') + 1;
    msgs_good = atoi(s);
}
