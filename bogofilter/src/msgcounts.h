/* $Id$ */

/*****************************************************************************

NAME:
   msgcounts.h -- routines for setting & computing .MSG_COUNT values

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#ifndef MSGCOUNTS_H
#define MSGCOUNTS_H

#include "lexer.h"

/* Globals */

#define	MSG_COUNT_MAX_LEN 100
extern	yylex_t	 read_msg_count_line;
extern	bool	 msgcount_more(void);

extern	char	*msg_count_text;
extern	int	 msg_count_leng;

/* Function prototypes */

void init_msg_counts(void);
void compute_msg_counts(void);
void set_msg_counts(char *s);

#endif	/* MSGCOUNTS_H */
