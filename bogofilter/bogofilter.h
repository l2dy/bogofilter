/* $Id$ */
/* 
 * $Log$
 * Revision 1.5  2002/09/23 11:31:53  m-a
 * Unnest comments, and move $ line down by one to prevent CVS from adding nested comments again.
 *
 * Revision 1.4  2002/09/23 10:08:49  m-a
 * Integrate patch by Zeph Hull and Clint Adams to present spamicity in
 * X-Spam-Status header in bogofilter -p mode.
 *
 * Revision 1.3  2002/09/18 22:30:22  relson
 * Created lexer.h with the definitions needed by lexer_l.l from bogofilter.h.
 * This removes the compile-time dependency between the two files.
 *
 * Revision 1.2  2002/09/15 19:07:12  relson
 * Add an enumerated type for return codes of RC_SPAM and RC_NONSPAM, which  values of 0 and 1 as called for by procmail.
 * Use the new codes and type for bogofilter() and when generating the X-Spam-Status message.
 *
 * Revision 1.1.1.1  2002/09/14 22:15:20  adrian_otto
 * 0.7.3 Base Source
 * */
/*  constants and declarations for bogofilter */

#include "lexer.h"

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
extern rc_t bogofilter(int fd, double *xss);
extern int bogodump(char *file);

extern wordlist_t ham_list, spam_list;
extern int verbose;

// end
