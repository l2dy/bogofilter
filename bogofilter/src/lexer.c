/* $Id$ */

/*
 * NAME
 *   lexer.c -- bogofilter's lexical analyzer (control routines)
 *
 *   01/01/2003 - split out of lexer.l
*/

#include "common.h"

#include <ctype.h>
#include <stdlib.h>

#include "base64.h"
#include "bogoreader.h"
#include "charset.h"
#include "error.h"
#include "lexer.h"
#include "mime.h"
#include "msgcounts.h"
#include "qp.h"
#include "textblock.h"
#include "token.h"
#include "word.h"
#include "xmalloc.h"

/* Global Variables */

int yylineno;
bool msg_header = true;
bool have_body  = false;
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

/* Function Prototypes */
static int yy_get_new_line(buff_t *buff);
static int skip_spam_header(buff_t *buff);
static int get_decoded_line(buff_t *buff);

/* Function Definitions */
 
extern char yy_get_state(void);
extern void yy_set_state_initial(void);

static void lexer_display_buffer(buff_t *buff)
{
    fprintf(dbgout, "*** %2d %c%c %2ld ",
	    yylineno-1, msg_header ? 'h' : 'b', yy_get_state(),
	    (long)(buff->t.leng - buff->read));
    buff_puts(buff, 0, dbgout);
    if (buff->t.leng > 0 && buff->t.text[buff->t.leng-1] != '\n')
	fputc('\n', dbgout);
}

/* Check for lines wholly composed of printable characters as they can cause a scanner abort 
   "input buffer overflow, can't enlarge buffer because scanner uses REJECT"
*/
static bool not_long_token(byte *buf, size_t count)
{
    size_t i;
    for (i=0; i < count; i += 1) {
	unsigned char c = (unsigned char)buf[i];
	if ((iscntrl(c) || isspace(c) || ispunct(c)) && (c != '_'))
	    return true;
    }
    return false;
}

static int yy_get_new_line(buff_t *buff)
{
    int count = (*reader_getline)(buff);
    const byte *buf = buff->t.text;

    static size_t hdrlen = 0;
    if (hdrlen==0)
	hdrlen=strlen(spam_header_name);

    if (count > 0)
	yylineno += 1;

    if (count == EOF) {
	if (ferror(fpin)) {
	    print_error(__FILE__, __LINE__, "input in flex scanner failed\n");
	    exit(EX_ERROR);
	} else {
	    return YY_NULL;
	}
    }

    /* Mime header check needs to be performed on raw input
    ** -- before mime decoding.  Without it, flex aborts:
    ** "fatal flex scanner internal error--end of buffer missed" */

    if (buff->t.leng > 2 &&
	buf[0] == '-' && buf[1] == '-' &&
	got_mime_boundary(&buff->t)) {
	yy_set_state_initial();
    }

    if (count >= 0 && DEBUG_LEXER(0))
	lexer_display_buffer(buff);
    
    /* skip spam_header ("X-Bogosity:") lines */
    while (msg_header
	   && count != EOF
	   && memcmp(buff->t.text,spam_header_name,hdrlen) == 0)
    {
	count = skip_spam_header(buff);
    }

    /* Also, save the text on a linked list of lines.
     * Note that we store fixed-length blocks here, not lines.
     * One very long physical line could break up into more
     * than one of these. */

    if (passthrough && passmode == PASS_MEM && count > 0)
	textblock_add(buff->t.text+buff->read, (size_t) count);

    return count;
}

static int skip_spam_header(buff_t *buff)
{
    while (true) {
	int count;
	buff->t.leng = 0;		/* discard X-Bogosity line */
	count = (*reader_getline)(buff);
	if (count <= 1 || !isspace(buff->t.text[0])) 
	    return count;
    }

    return EOF;
}

static int get_decoded_line(buff_t *buff)
{
    size_t used = buff->t.leng;
    byte *buf = buff->t.text + used;
    int count = yy_get_new_line(buff);

    if (count == EOF) {
	if (ferror(fpin)) {
	    print_error(__FILE__, __LINE__, "input in flex scanner failed\n");
	    exit(EX_ERROR);
	} else {
	    return YY_NULL;
	}
    }

    if (0) { /* debug */
	fprintf(dbgout, "%d: ", count);
	buff_puts(buff, 0, dbgout);
	fprintf(dbgout, "\n");
    }

    if ( ! msg_header && msg_state->mime_type != MIME_TYPE_UNKNOWN)
    {
	word_t line;
	uint decoded_count;

        line.leng = (uint) (buff->t.leng - used);
	line.text = buff->t.text + used;
	decoded_count = mime_decode(&line);
	/*change buffer size only if the decoding worked */
	if (decoded_count != 0 && decoded_count < (uint) count) {
	    buff->t.leng -= (uint) (count - decoded_count);
	    count = decoded_count;
	    if (DEBUG_LEXER(1)) 
		lexer_display_buffer(buff);
	}
    }

    /* CRLF -> NL */
    if (count >= 2 && memcmp((char *) buf + count - 2, CRLF, 2) == 0) {
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

    if (!msg_count_file)
	lexer = &v3_lexer;
}

int yyinput(byte *buf, size_t max_size)
/* input getter for the scanner */
{
    int i, cnt;
    int count = 0;
    buff_t buff;

    buff_init(&buff, buf, 0, max_size);

    /* After reading a line of text, check if it has special characters.
     * If not, trim some, but leave enough to match a max length token.
     * Then read more text.  This will ensure that a really long sequence
     * of alphanumerics, which bogofilter will ignore anyway, doesn't crash
     * the flex lexer.
     */

    while ((cnt = get_decoded_line(&buff)) != 0) {
	count += cnt;
	if ((count <= (MAXTOKENLEN * 1.5)) || not_long_token(buff.t.text, count))
	    break;

	if (count >= MAXTOKENLEN * 2) {
	    size_t shift = count - MAXTOKENLEN;
	    memmove(buff.t.text, buff.t.text + shift, MAXTOKENLEN+D);
	    count = MAXTOKENLEN;
	    buff.t.leng = MAXTOKENLEN;
	}
    }

    for (i = 0; i < count; i++ )
    {
	byte ch = buf[i];
	buf[i] = charset_table[ch];
    }

    return (count == EOF ? 0 : count);
}

size_t text_decode(word_t *w)
{
    char *beg = (char *) w->text;
    char *fin = beg + w->leng;
    char *txt = strstr(beg, "=?");		/* skip past leading text */
    size_t size = txt - beg;

    while (txt < fin) {
	word_t n;
	char *typ = strchr(txt+2, '?');		/* Encoding type - 'B' or 'Q' */
	char *tmp = typ + 3;
	char *end = strstr(tmp, "?=");		/* last char of encoded word  */
	size_t len = end - tmp;
	bool copy;

	n.text = (byte *)tmp;			/* Start of encoded word */
	n.leng = len;				/* Length of encoded word */
	n.text[n.leng] = '\0';

	if (DEBUG_LEXER(2)) {
	    fputs("***  ", dbgout);
	    word_puts(&n, 0, dbgout);
	    fputs("\n", dbgout);
	}

	switch (tolower(typ[1])) {		/* ... encoding type */
	case 'b':
	    if (base64_validate(&n))
		len = base64_decode(&n);	/* decode base64 */
	    break;
	case 'q':
	{
	    if (qp_validate(&n))
		len = qp_decode(&n);		/* decode quoted-printable */
	    break;
	}
	}

	n.leng = len;
	n.text[n.leng] = '\0';

	if (DEBUG_LEXER(3)) {
	    fputs("***  ", dbgout);
	    word_puts(&n, 0, dbgout);
	    fputs("\n", dbgout);
	}

	memmove(beg+size, n.text, len+1);
	size += len;
	txt = end + 2;
	if (txt >= fin)
	    break;
	end = strstr(txt, "=?");
	copy = end != NULL;

	if (copy) {
	    tmp = txt;
	    while (copy && tmp < end) {
		if (isspace((unsigned char)*tmp))
		    tmp += 1;
		else
		    copy = false;
	    }
	}

	if (copy)
	    txt = end;
	else while (txt < end)
	    beg[size++] = *txt++;
    }

    return size;
}

/*
 * The following sets edit modes for GNU EMACS
 * Local Variables:
 * mode:c
 * End:
 */
