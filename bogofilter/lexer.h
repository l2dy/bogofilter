
// length of *yylval will not exceed this...
#define MAXTOKENLEN	30

// lexer interface
#define TOKEN	1	// Ordinary token
#define FROM	2	// Mail message delimiter

extern FILE	*yyin;

extern char *yylval;

struct textblock
{
    char		*block;
    struct textblock	*next;
};

extern struct textblock textblocks, *textend;

extern int get_token(void);

