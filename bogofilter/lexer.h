// discard tokens longer than this, so as not to clutter up our
// wordlists with a lot of MIME-enclosure cruft.
#define MAXWORDLEN	20

// lexer interface
#define TOKEN	1	// Ordinary token
#define FROM	2	// Mail message delimiter

extern FILE	*yyin;
extern char	*yytext;

struct textblock
{
    char		*block;
    struct textblock	*next;
};

extern struct textblock textblocks, *textend;

extern int get_token(void);

