/* $Id$ */

// length of *yylval will not exceed this...
#define MAXTOKENLEN	30

// lexer interface
enum {
    TOKEN = 1,	// regular token
    FROM,	// mbox message delimiter
    BOUNDARY,	// MIME multipart boundary line
    EMPTY	// empty line
};

extern FILE	*yyin;

extern char *yylval;

struct textblock
{
    char		*block;
    /*@owned@*/ struct textblock	*next;
    size_t		len;
};

extern struct textblock textblocks, *textend;

extern int myfgets(char *buf, int max_size, FILE *s);
extern int get_token(void);

