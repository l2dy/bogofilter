/* $Id$ */

#ifndef	LEXER_H
#define	LEXER_H

/* length of *yylval will not exceed this... */
#define MAXTOKENLEN	30

/* lexer interface */
typedef enum {
    NONE = 0,
    TOKEN,	/* regular token */
    FROM,	/* mbox message delimiter */
    BOUNDARY,	/* MIME multipart boundary line */
    IPADDR,	/* ip address */
    CHARSET,	/* charset="..." */
    EMPTY	/* empty line */
} token_t;

extern char *yylval;

struct textblock
{
    char		*block;
    /*@owned@*/ struct textblock	*next;
    size_t		len;
};

extern bool block_on_subnets;
extern struct textblock textblocks, *textend;

extern int lexer_fgets(char *buf, int max_size, FILE *s);
extern token_t get_token(void);
extern token_t yylex(void);

#endif	/* LEXER_H */
