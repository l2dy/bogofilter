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
#include "msgcounts.h"
#include "textblock.h"
#include "token.h"
#include "word.h"
#include "xmalloc.h"

/* Global Variables */

int yylineno;
bool msg_header = true;
lexer_t *lexer = NULL;

/* Local Variables */

extern char *spam_header_name;

lexer_t v3_lexer = {
    lexer_v3_lex,
    &lexer_v3_text,
    &lexer_v3_leng
};

lexer_t msg_count_lexer = {
    read_msg_count_line,
    &msg_count_text,
    &msg_count_leng
};

#define YY_NULL 0

/* Function Prototypes */
static int yy_get_new_line(buff_t *buff);
static int skip_spam_header(buff_t *buff);
static int get_decoded_line(buff_t *buff);

/* Function Definitions */

static void lexer_display_buffer(buff_t *buff)
{
    fprintf(dbgout, "*** %2d %c %ld ", yylineno,
	    msg_header ? 'h' : 'b', (long)(buff->t.leng - buff->read));
    buff_puts(buff, 0, dbgout);
    if (buff->t.leng > 0 && buff->t.text[buff->t.leng-1] != '\n')
	fputc('\n', dbgout);
}

bool is_from(const byte *text, size_t leng)
{
    if (leng >= 5 && memcmp(text, "From ", 5) == 0)
	return true;
    return false;
}

/* Check for lines wholly composed of printable characters as they can cause a scanner abort 
   "input buffer overflow, can't enlarge buffer because scanner uses REJECT"
*/
static bool may_be_long_token(byte *buf, size_t count)
{
    size_t i;
    for (i=0; i < count; i += 1) {
	unsigned char c = (unsigned char)buf[i];
	if ((iscntrl(c) || isspace(c) || ispunct(c)) && (c != '_'))
	    return false;
    }
    return true;
}

static int lgetsl(buff_t *buff)
{
    int count = 0;

    count += buff_fgetsl(buff, fpin);
    yylineno += 1;

    /* Special check for message separator.
       If found, handle it immediately.
    */

    if (is_from(buff->t.text+buff->read, buff->t.leng-buff->read))
	got_from();

    if (count >= 0 && DEBUG_LEXER(0))
	lexer_display_buffer(buff);

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
	textblock_add(textblocks, buff->t.text+buff->read, count);

    return count;
}

static int skip_spam_header(buff_t *buff)
{
    while (true) {
	int count;
	buff->t.leng = 0;		/* discard X-Bogosity line */
	count = lgetsl(buff);
	if (count <= 1 || !isspace(buff->t.text[0])) 
	    return count;
    }

    return -1;
}

static int get_decoded_line(buff_t *buff)
{
    int count;
    size_t used = buff->t.leng;
    byte *buf = buff->t.text + used;

    count = yy_get_new_line(buff);

/* unfolding:
** 	causes "^\tid" to be treated as continuation of previous line
** 	hence doesn't match lexer pattern which specifies beginning of line
*/
#if 0
    while (0 && msg_header) {
	int c = fgetc(fpin);
	if (c == EOF)
	    break;
	if ((c == ' ') || (c == '\t')) {
	    int add;
	    /* continuation line */
	    ungetc(c,fpin);
	    add = lgetsl(buff);
	    if (add == EOF) break;
	    if (passthrough && passmode == PASS_MEM && buff->t.leng > 0)
		textblock_add(textblocks, buff->t.text+buff->read, buff->t.leng);
	    count += buff->t.leng;
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
	&& ! msg_header
	&& msg_state->mime_type != MIME_TYPE_UNKNOWN)
    {
	word_t line;
	int decoded_count;

        line.leng = buff->t.leng - used;
	line.text = buff->t.text + used;
	decoded_count = mime_decode(&line);
	/*change buffer size only if the decoding worked */
	if (decoded_count != 0 && decoded_count < count) {
	    buff->t.leng -= count - decoded_count;
	    count = decoded_count;
	    if (DEBUG_LEXER(1)) 
		lexer_display_buffer(buff);
	}
    }

    /* \r\n -> \n */
    if (count >= 2 && 0 == memcmp((const char *)(buf + count - 2), "\r\n", 2)) {
	count --;
	*(buf + count - 1) = '\n';
    }

    if (buff->t.leng < buff->size)	/* for easier debugging - removable */
	Z(buff->t.text[buff->t.leng]);	/* for easier debugging - removable */

    return count;
}

int buff_fill(buff_t *buff, size_t used, size_t need)
{
    int cnt = 0;
    size_t leng = buff->t.leng;
    size_t size = buff->size;

    /* check bytes needed vs. bytes in buff */
    while (size - leng > 2 && need > leng - used) {
	/* too few, read more */
	int add = get_decoded_line(buff);
	if (add == EOF) return EOF;
	if (add == 0) break ;
	cnt += add;
	leng += add;
    }
    return cnt;
}

void yyinit(void)
{
    yylineno = 0;
    msg_header = true;

    if (!msg_count_file)
	lexer = &v3_lexer;
}

int yyinput(byte *buf, size_t max_size)
/* input getter for the scanner */
{
    int i, count = 0;
    buff_t buff;

    bool done = false;

    buff_init(&buff, buf, 0, max_size);

    /* After reading a line of text, check if it has special characters.
     * If not, trim some, but leave enough to match a max  length token.
     * Then read more text.  This will ensure that a really long sequence
     * of alphanumerics, which bogofilter will ignore anyway, doesn't crash
     * the flex lexer.
     */

    while (!done) {
	done = true;
	count += get_decoded_line(&buff);

	while (count > (MAXTOKENLEN * 1.5) && may_be_long_token(buff.t.text, count)) {
	    done = false;
	    if (count < MAXTOKENLEN * 2) 
		break;
	    else {
		size_t shift = count - MAXTOKENLEN;
		memcpy(buff.t.text, buff.t.text + shift, MAXTOKENLEN+D);
		count = MAXTOKENLEN;
		buff.t.leng = MAXTOKENLEN;
	    }	
	}
    }

    /* do nothing if in header */

    if ((count > 0)
	&& ! msg_header
	&& msg_state->mime_type == MIME_TEXT_HTML)
    {
	count = process_html_comments(&buff);
    }

    for (i = 0; i < count; i++ )
    {
	byte ch = buf[i];
	buf[i] = charset_table[ch];
    }

    return (count == -1 ? 0 : count);
}

/*
 * The following sets edit modes for GNU EMACS
 * Local Variables:
 * mode:c
 * End:
 */
