/* $Id$ */

/*****************************************************************************

NAME:
   lexer.h -- prototypes and definitions for lexer.c

******************************************************************************/

#ifndef	LEXER_H
#define	LEXER_H

#include "buff.h"
#include "word.h"

extern FILE *yyin;

extern	bool	block_on_subnets;

extern	bool	kill_html_comments;
extern	int	count_html_comments;
extern	bool	score_html_comments;

/* lexer interface */
typedef enum {
    NONE = 0,
    TOKEN,	/* regular token */
    FROM,	/* mbox message delimiter */
    BOUNDARY,	/* MIME multipart boundary line */
    EMPTY,	/* empty line */
    IPADDR,	/* ip address */
} token_t;

/* in lexer.c */
extern int yylineno;
extern int msg_header;
extern bool is_from(word_t *w);

/* in lexer_v3.l */
extern token_t	lexer_v3_lex(void);
extern int	lexer_v3_leng;
extern char   * lexer_v3_text;

/* in lexer.c */
extern int yyinput(byte *buf, size_t size);
extern int yyredo(word_t *text, char del);

extern int buff_fill(buff_t *buff, size_t used, size_t need);

#endif	/* LEXER_H */
