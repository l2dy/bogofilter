/* $Id$ */

/*
 * NAME
 *   lexer.c -- bogofilter's lexical analyzer (control routines)
 *
 *   01/01/2003 - split out of lexer.l
*/

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>

#include <config.h>
#include "common.h"

#include "charset.h"
#include "error.h"
#include "html.h"
#include "lexer.h"
#include "mime.h"
#include "textblock.h"
#include "token.h"
#include "word.h"
#include "xmalloc.h"

/* Local Variables */

int yylineno;
int msg_header = 1;

static buff_t *yysave = NULL;

extern char *spam_header_name;

#define YY_NULL 0

/* Function Prototypes */
static int yy_use_redo_text(buff_t *buff);
static int yy_get_new_line(buff_t *buff);
static int skip_spam_header(buff_t *buff);
static int get_decoded_line(buff_t *buff);
static void check_alphanum(buff_t *buff);

/* Function Definitions */

static void lexer_display_buffer(buff_t *buff)
{
    fprintf(dbgout, "*** %2d %c,%c %d ", yylineno,
	    HORB(msg_header), HORB(msg_state->mime_header), buff->t.leng);
    buff_puts(buff, 0, dbgout);
    if (buff->t.leng > 0 && buff->t.text[buff->t.leng-1] != '\n')
	fputc('\n', dbgout);
}

static void check_alphanum(buff_t *buff)
{
    size_t e = 0;
    size_t i;
    size_t l = buff->t.leng;
    byte *txt = buff->t.text;

    if (l < MAXTOKENLEN)
	return;
    for (i = 0; i < buff->t.leng; i += 1) {
	if (isalnum((unsigned char) txt[i]))
	    e = i;
	else
	    break;
    }
    if (e > MAXTOKENLEN) {
	memcpy(txt, txt+e, l-e);
	buff->t.leng -= e;
    }
}

bool is_from(word_t *w)
{
    return (w->leng >= 5 && memcmp(w->text, "From ", 5) == 0);
}

static int lgetsl(buff_t *buff)
{
    int count = buff_fgetsl(buff, fpin);
    yylineno += 1;
    if (count >= 0 && DEBUG_LEXER(0)) {
	lexer_display_buffer(buff);
    }

    /* Special check for message separator.
       If found, handle it immediately.
    */

    if (is_from(&buff->t)) {
	got_from();
    }

    return count;
}

static int yy_use_redo_text(buff_t *buff)
{
    size_t used  = buff->t.leng;
    size_t size  = buff->size;
    size_t avail = size - used;
    byte  *buf = buff->t.text+used;
    size_t count = min(yysave->size, avail-2);

    memcpy(buf, yysave->t.text, count );
    buf[count++] = '\n';
    buf[count] = '\0';
    buff->t.leng += count;
    buff_free(yysave);
    yysave = NULL;
    return count;
}

static int yy_get_new_line(buff_t *buff)
{
    int count = lgetsl(buff);

    static size_t hdrlen = 0;
    if (hdrlen==0)
	hdrlen=strlen(spam_header_name);

    /* skip spam_header ("X-Bogosity:") lines */
    while (msg_header
	   && count != -1
	   && memcmp(buff->t.text,spam_header_name,hdrlen) == 0)
    {
	count = skip_spam_header(buff);
    }

    /* Also, save the text on a linked list of lines.
     * Note that we store fixed-length blocks here, not lines.
     * One very long physical line could break up into more
     * than one of these. */

    if (passthrough && passmode == PASS_MEM && count > 0)
	textblock_add(textblocks, (const char *)buff->t.text+buff->read, count);

    return count;
}

static int skip_spam_header(buff_t *buff)
{
    while (true) {
	int count;
	buff->t.leng = 0;		/* discard X-Bogosity line */
	count = lgetsl(buff);
	if (count <= 0 || !isspace(buff->t.text[0])) 
	    return count;
    }

    return -1;
}

static int get_decoded_line(buff_t *buff)
{
    int count;
    size_t used = buff->t.leng;
    byte *buf;

    buff_shrink(buff, used);

    buf = buff->t.text;

    if (yysave == NULL)
	count = yy_get_new_line(buff);
    else
	count = yy_use_redo_text(buff);

/* unfolding:
** 	causes "^\tid" to be treated as continuation of previous line
** 	hence doesn't match lexer pattern which specifies beginning of line
*/
#if 0
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
#endif

    if (count == -1) {
	if (ferror(fpin)) {
	    print_error(__FILE__, __LINE__, "input in flex scanner failed\n");
	    exit(2);
	} else {
	    return YY_NULL;
	}
    }

    if (0) { /* debug */
	fprintf(dbgout, "%d: ", count);
	buff_puts(buff, 0, dbgout);
	fprintf(dbgout, "\n");
    }

    if (count >= 5
	&& memcmp("From ", buf, 5) != 0
	&& !msg_header && !msg_state->mime_header
	&& msg_state->mime_type != MIME_TYPE_UNKNOWN) {
	int decoded_count = mime_decode(&buff->t);
	/*change buffer size only if the decoding worked */
	if (decoded_count != 0 && decoded_count < count) {
	    buff->t.leng = count = decoded_count;
	    check_alphanum(buff);
	    memcpy(buf, buff->t.text, buff->t.leng);
	    if (DEBUG_LEXER(1)) 
		lexer_display_buffer(buff);
	}
    }

    /* \r\n -> \n */
    if (count >= 2 && 0 == memcmp((const char *)(buf + count - 2), "\r\n", 2)) {
	count --;
	*(buf + count - 1) = '\n';
    }

    buff_expand(buff, used);

    if (buff->t.leng < buff->size)	/* debug code - remove */
	Z(buff->t.text[buff->t.leng]);	/* debug code - remove */

    return count;
}

int buff_fill(size_t need, buff_t *buff)
{
    int cnt = 0;
    size_t used = buff->t.leng;
    size_t size = buff->size;

    /* check bytes needed vs. bytes in buff */
    while (size-used > 2 && need > used) {
	/* too few, read more */
	int add = get_decoded_line(buff);
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
    buff_t *buff = buff_new(buf, 0, max_size);
    count = get_decoded_line(buff);

    /* do nothing if in header */

    if ((count > 0)
	&& ! (msg_header || msg_state->mime_header)
	&& msg_state->mime_type == MIME_TEXT_HTML)
    {
	if (kill_html_comments || score_html_comments )
	    count = process_html_comments(buff);
    }

    for (i = 0; i < count; i++ )
    {
	byte ch = buf[i];
	buf[i] = charset_table[ch];
    }
    
    buff_free(buff);

    return (count == -1 ? 0 : count);
}

int yyredo(word_t *text, char del)
{
    const byte *t = text->text;
    size_t leng = text->leng;
    buff_t *tmp;
    byte *d;
    size_t tlen = strlen((const char *)t);
    assert(tlen == leng);

    if (DEBUG_LEXER(2)) fprintf(dbgout, "yyredo:  %d \"%s\"\n", leng, text->text);

    /* if already processing saved text, concatenate new after old */
    if (yysave == NULL) {
	tmp = buff_new(xmalloc(tlen+D), tlen, tlen+D);
	memcpy(tmp->t.text, t, tlen);
	Z(tmp->t.text[tmp->t.leng]);	/* debug code - remove */
    } else {
	size_t yu = yysave->t.leng;
	size_t nlen = yu + tlen;
	tmp = buff_new(xmalloc(nlen+D), nlen, nlen+D);
	memcpy(tmp->t.text, yysave->t.text, yu);
	memcpy(tmp->t.text+yu, t, tlen);
	Z(tmp->t.text[tmp->t.leng]);	/* debug code - remove */
	xfree(yysave->t.text);
	buff_free(yysave);
    }

    yysave = tmp;
    if ((d = memchr(yysave->t.text, del, yysave->size)) != NULL)
	*d = ' ';

    return 1;
}

/*
 * The following sets edit modes for GNU EMACS
 * Local Variables:
 * mode:c
 * End:
 */
