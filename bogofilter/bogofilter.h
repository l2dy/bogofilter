/* $Id$ */
/* $Log$
 * Revision 1.1  2002/09/14 22:15:20  adrian_otto
 * Initial revision
 * */
//  constants and declarations for bogofilter

typedef struct 
{
    char *name;			// resource name (for debug/verbose messages)
    void *db;			// database handle 
    unsigned long msgcount;	// count of messages in wordlist.
    char *file;			// associated  file
    char *count_file;           // file for counting emails
}
wordlist_t;

extern int read_list(wordlist_t *list);
extern void write_list(wordlist_t *ham_list);
extern void register_words(int fd, wordlist_t *list, wordlist_t *other);
extern int get_token(void);
extern void lexer_stream_mode(void);
extern int bogofilter(int fd);
extern int bogodump(char *file);

extern wordlist_t ham_list, spam_list;
extern int verbose;
extern int status;

// discard tokens longer than this, so as not to clutter up our
// wordlists with a lot of MIME-enclosure cruft.
#define MAXWORDLEN	20

// lexer interface
#define TOKEN	1	// Ordinary token
#define FROM	2	// Mail message delimiter
extern FILE	*yyin;
extern char *yytext;

struct textblock
{
    char		*block;
    struct textblock	*next;
};

extern struct textblock textblocks, *textend;

// end
