/* $Id$ */

/*
 * NAME
 *   lexer.c -- bogofilter's lexical analyzer (control routines)
 *
 *   01/01/2003 - split out of lexer.l
*/

#include <stdlib.h>
#include <ctype.h>

#include <config.h>
#include "common.h"

#include "charset.h"
#include "error.h"
#include "html.h"
#include "lexer.h"
#include "mime.h"
#include "fgetsl.h"
#include "textblock.h"
#include "token.h"
#include "xmalloc.h"
#include "xstrdup.h"

/* Local Variables */

char *yylval;

int yylineno;
int msg_header = 1;

static char *yysave = NULL;

extern char *spam_header_name;

#define YY_NULL 0

/* Function Prototypes */
static int yy_use_redo_text(byte *buf, size_t max_size);
static int yy_get_new_line(byte *buf, size_t max_size);
static int skip_spam_header(byte *buf, size_t max_size);
extern int get_decoded_line(byte *buf, size_t size);


/* Function Definitions */
static int lgetsl(byte *buf, size_t size)
{
    size_t count = fgetsl((char *)buf, size, fpin);
    yylineno += 1;
    if (DEBUG_LEXER(0)) fprintf(dbgout, "*** %2d %d %s\n", yylineno, msg_header, buf);

    /* Special check for message separator.
       If found, handle it immediately.
    */

    if (memcmp(buf, "From ", 5) == 0)
	got_from();

    return count;
}

static int yy_use_redo_text(byte *buf, size_t max_size)
{
    size_t count;

    count = strlcpy((char *)buf, yysave, max_size-2);
    buf[count++] = '\n';
    buf[count] = '\0';
    xfree(yysave);
    yysave = NULL;
    return count;
}

static int yy_get_new_line(byte *buf, size_t max_size)
{
    int count = lgetsl(buf, max_size);

    static size_t hdrlen = 0;
    if (hdrlen==0)
	hdrlen=strlen(spam_header_name);

    /* skip spam_header ("X-Bogosity:") lines */
    while (msg_header
	   && count != -1
	   && memcmp(buf,spam_header_name,hdrlen) == 0)
    {
	count = skip_spam_header(buf, max_size);
    }

    /* Also, save the text on a linked list of lines.
     * Note that we store fixed-length blocks here, not lines.
     * One very long physical line could break up into more
     * than one of these. */

    if (passthrough && count > 0)
	textblock_add(textblocks, (const char *)buf, count);

    return count;
}

static int skip_spam_header(byte *buf, size_t max_size)
{
    int count;

    do {
	count = lgetsl(buf, max_size);
	if (count != -1 && *buf == '\n')
	    break;
    } while (count != -1 && isspace(*buf));

    return count;
}

int get_decoded_line(byte *buf, size_t max_size)
{
    int count;

    if (yysave == NULL)
	count = yy_get_new_line(buf, max_size);
    else
	count = yy_use_redo_text(buf, max_size);
	
/* unfolding:
** 	causes "^\tid" to be treated as continuation of previous line
** 	hence doesn't match lexer pattern which specifies beginning of line
*/
    while (0 && msg_header) {
	int c = fgetc(fpin);
	if (c == EOF)
	    break;
	if ((isspace(c))) {
	    int add;
	    /* continuation line */
	    ungetc(c,fpin);
	    if (buf[count - 1] == '\n') count --;
	    add = lgetsl(buf + count, max_size - count);
	    if (add == EOF) break;
	    if (passthrough)
		textblock_add(textblocks, (const char *)(buf+count), add);
	    count += add;
	} else {
	    ungetc(c,fpin);
	    break;
	}
    }

    if (count == -1) {
	if (ferror(fpin)) {
	    print_error(__FILE__, __LINE__, "input in flex scanner failed\n");
	    exit(2);
	} else {
	    return YY_NULL;
	}
    }

    if (0) { /* debug */
	fprintf(stderr, "%d: ", count);
	fwrite(buf, 1, count, stderr);
	fprintf(stderr, "\n");
    }

    if (count > 0
	&& memcmp("From ", buf, 5) != 0
	&& !msg_header && !msg_state->mime_header) {
	int decoded_count = mime_decode(buf, count);
	/*change buffer size only if the decoding worked */
	if (decoded_count != 0)
	    count = decoded_count;
    }

    /* \r\n -> \n */
    if (count >= 2 && 0 == strcmp((const char *)(buf + count - 2), "\r\n")) {
	count --;
	*(buf + count - 1) = '\n';
    }

    return count;
}

int buff_fill(size_t need, byte *buf, size_t used, size_t size)
{
    int cnt = 0;
    /* check bytes needed vs. bytes in buff */
    while (size-used > 2 && need > used) {
	/* too few, read more */
	int add = get_decoded_line(buf+used, size-used);
	if (add == EOF) return EOF;
	if (add == 0) break ;
	cnt += add;
	used += add;
    }
    return cnt;
}

int yyinput(byte *buf, size_t max_size)
/* input getter for the scanner */
{
    int i, count;

    count = get_decoded_line(buf, max_size);

    /* do nothing if in header */

    if ((count > 0)
	&& ! (msg_header || msg_state->mime_header)
	&& msg_state->mime_type == MIME_TEXT_HTML)
    {
	if (kill_html_comments || score_html_comments )
	    count = process_html_comments(buf, count, max_size);
    }

    for (i = 0; i < count; i++ )
    {	
	byte ch = buf[i];
	buf[i] = charset_table[ch];
    }
    
    return (count == -1 ? 0 : count);
}

int yyredo(const byte *text, char del)
{
    char *tmp;

    if (DEBUG_LEXER(1)) fprintf(dbgout, "yyredo:  %p %s\n", yysave, text );

    /* if already processing saved text, concatenate new after old */
    if (yysave == NULL) {
	tmp = xstrdup((const char *)text);
    } else {
	size_t t = strlen((const char *)text);
	size_t s = strlen(yysave);
	tmp = xmalloc(s + t + 1);
	memcpy(tmp, yysave, s+1);
	memcpy(tmp+s, text, t+1);
    }

    xfree(yysave);
    yysave = tmp;
    if ((tmp = strchr(tmp,del)) != NULL)
	    *tmp++ = ' ';

    return 1;
}

/*
 * The following sets edit modes for GNU EMACS
 * Local Variables:
 * mode:c
 * End:
 */
