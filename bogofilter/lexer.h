/* $Id$ */

/*****************************************************************************

NAME:
   lexer.h -- prototypes and definitions for lexer.c

******************************************************************************/

#ifndef	LEXER_H
#define	LEXER_H

extern FILE *yyin;
extern int yyleng;
extern char *yytext;
extern char *yylval;

extern bool block_on_subnets;

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

/* in lexer_head.l */
extern token_t	lexer_lex(void);
extern int	lexer_leng;
extern char   * lexer_text;

/* in lexer_text_plain.l */
extern token_t	text_plain_lex(void);
extern int	text_plain_leng;
extern char   * text_plain_text;

/* in lexer_text_html.l */
extern token_t	text_html_lex(void);
extern int	text_html_leng;
extern char   * text_html_text;

/* in lexer.c */
extern int yygetline(char *buf, int max_size);
extern int yyinput(char *buf, int max_size);
extern int yyredo(const char *text, char del);

#endif	/* LEXER_H */
