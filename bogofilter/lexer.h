/* $Id$ */

/* length of *yylval will not exceed this... */
#define MAXTOKENLEN	30

/* lexer interface */
typedef enum {
    TOKEN = 1,	/* regular token */
    FROM,	/* mbox message delimiter */
    BOUNDARY,	/* MIME multipart boundary line */
    EMPTY	/* empty line */
} token_t;

extern char *yylval;

struct textblock
{
    char		*block;
    /*@owned@*/ struct textblock	*next;
    size_t		len;
};

extern struct textblock textblocks, *textend;

extern int lexer_fgets(char *buf, int max_size, FILE *s);
extern token_t get_token(void);

