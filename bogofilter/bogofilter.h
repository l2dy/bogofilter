/* $Id$ */
/* $Log$
 * Revision 1.2  2002/09/15 19:07:12  relson
 * Add an enumerated type for return codes of RC_SPAM and RC_NONSPAM, which  values of 0 and 1 as called for by procmail.
 * Use the new codes and type for bogofilter() and when generating the X-Spam-Status message.
 *
/* Revision 1.1.1.1  2002/09/14 22:15:20  adrian_otto
/* 0.7.3 Base Source
/* */
//  constants and declarations for bogofilter

typedef enum rc_e {RC_SPAM=0, RC_NONSPAM=1}  rc_t;

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
extern rc_t bogofilter(int fd);
extern int bogodump(char *file);

extern wordlist_t ham_list, spam_list;
extern int verbose;

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

// end
