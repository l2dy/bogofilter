/* $Id$ */

/*****************************************************************************

NAME:
   msgcounts.c -- routines for setting & computing .MSG_COUNT values

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#include <ctype.h>
#include <stdlib.h>

#include <config.h>
#include "common.h"

#include "msgcounts.h"
#include "wordlists.h"

/* Globals */

long msgs_good = 0L;
long msgs_bad  = 0L;

/* Function Definitions */

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

