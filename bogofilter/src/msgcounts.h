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

extern	bool	msg_count_file;

#define	MSG_COUNT_MAX_LEN 100
extern	yylex_t	 msg_count_lex;
extern	char	*msg_count_text;
extern	size_t	 msg_count_leng;

extern	long	 msgs_good;
extern 	long	 msgs_bad;

/* Function prototypes */

void init_msg_counts(void);
void compute_msg_counts(void);
void set_msg_counts(char *s);

#endif	/* MSGCOUNTS_H */
