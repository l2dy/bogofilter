/* $Id$ */
/*
 * $Log$
 * Revision 1.4  2002/10/04 04:01:51  relson
 * Added cvs keywords Id and Log to the files' headers.
 *
 */

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

