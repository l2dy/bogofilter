/* $Id$ */

/**
 * \file lexer.c
 * bogofilter's lexical analyzer (control routines)
 *
 * \date 2003-01-01 split out of lexer.l
 */

#include "common.h"

#include <ctype.h>
#include <stdlib.h>

#include "base64.h"
#include "bogoconfig.h"
#include "bogoreader.h"
#include "charset.h"
#include "error.h"
#include "lexer.h"
#include "memstr.h"
#include "mime.h"
#include "msgcounts.h"
#include "qp.h"
#include "textblock.h"
#include "token.h"
#include "word.h"
#include "xmalloc.h"

/* Global Variables */

extern int yylineno;

bool msg_header = true;
bool have_body  = false;
lexer_t *lexer = NULL;

/* Local Variables */

static lexer_t v3_lexer = {
    yylex,
    &yytext,
    &yyleng
};

lexer_t msg_count_lexer = {
    read_msg_count_line,
    &msg_count_text,
    &msg_count_leng
};

/* Function Prototypes */
static int yy_get_new_line(buff_t *buff);
static int get_decoded_line(buff_t *buff);
static int skip_folded_line(buff_t *buff);

/* Function Definitions */

void lexer_init(void)
{
    mime_reset();
    token_init();
    lexer_v3_init(NULL);
    init_charset_table(charset_default, true);
}

static void lexer_display_buffer(buff_t *buff)
{
    fprintf(dbgout, "*** %2d %c%c %2ld ",
	    yylineno-1, msg_header ? 'h' : 'b', yy_get_state(),
	    (long)(buff->t.leng - buff->read));
    buff_puts(buff, 0, dbgout);
    if (buff->t.leng > 0 && buff->t.text[buff->t.leng-1] != '\n')
       fputc('\n', dbgout);
}

/*
 * Check for lines wholly composed of printable characters as they can
 * cause a scanner abort
 * "input buffer overflow, can't enlarge buffer because scanner uses
 * REJECT"
 */
static bool not_long_token(byte *buf, uint count)
{
    uint i;
    for (i=0; i < count; i += 1) {
       byte c = buf[i];
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
       if (fpin == NULL || !ferror(fpin)) {
	   return YY_NULL;
       }
       else {
	   print_error(__FILE__, __LINE__, "input in flex scanner failed\n");
	   exit(EX_ERROR);
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
/* don't skip if inside message/rfc822 */
	   && msg_state == msg_state->parent
	   && memcmp(buff->t.text,spam_header_name,hdrlen) == 0) {
       count = skip_folded_line(buff);
    }

    /* Also, save the text on a linked list of lines.
     * Note that we store fixed-length blocks here, not lines.
     * One very long physical line could break up into more
     * than one of these. */

    if (passthrough && passmode == PASS_MEM && count > 0)
       textblock_add(buff->t.text+buff->read, (size_t) count);

    return count;
}

static int get_decoded_line(buff_t *buff)
{
    uint used = buff->t.leng;
    byte *buf = buff->t.text + used;
    int count = yy_get_new_line(buff);

    if (count == EOF) {
       if ( !ferror(fpin))
	   return YY_NULL;
       else {
	   print_error(__FILE__, __LINE__, "input in flex scanner failed\n");
	   exit(EX_ERROR);
       }
    }

#ifdef EXCESSIVE_DEBUG
    /* debug */
    fprintf(dbgout, "%d: ", count);
    buff_puts(buff, 0, dbgout);
    fprintf(dbgout, "\n");
#endif

    if ( !msg_header && msg_state->mime_type != MIME_TYPE_UNKNOWN)
    {
       word_t line;
       uint decoded_count;
       line.leng = (uint) (buff->t.leng - used);
       line.text = buff->t.text + used;

       decoded_count = mime_decode(&line);
       /*change buffer size only if the decoding worked */
       if (decoded_count != 0 && decoded_count < (uint) count) {
	   buff->t.leng -= (uint) (count - decoded_count);
	   count = (int) decoded_count;
	   if (DEBUG_LEXER(1))
	       lexer_display_buffer(buff);
       }
    }

    /* CRLF -> NL */
    if (count >= 2 && memcmp(buf + count - 2, CRLF, 2) == 0) {
       count --;
       *(buf + count - 1) = (byte) '\n';
    }

    if (buff->t.leng < buff->size)     /* for easier debugging - removable */
       Z(buff->t.text[buff->t.leng]);  /* for easier debugging - removable */

    return count;
}

static int skip_folded_line(buff_t *buff)
{
    for(;;) {
       int count;
       buff->t.leng = 0;
       count = reader_getline(buff);
       yylineno += 1;
       if (!isspace(buff->t.text[0]))
	   return count;
       /* Check for empty line which terminates message header */
       if (is_eol((char *)buff->t.text, count))
	   return count;
    }

/*    return EOF; */
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

    if ( !msg_count_file)
       lexer = &v3_lexer;
}

int yyinput(byte *buf, size_t used, size_t size)
/* input getter for the scanner */
{
    int i, cnt;
    int count = 0;
    buff_t buff;

    buff_init(&buff, buf, 0, (uint) size);

    /* After reading a line of text, check if it has special characters.
     * If not, trim some, but leave enough to match a max length token.
     * Then read more text.  This will ensure that a really long sequence
     * of alphanumerics, which bogofilter will ignore anyway, doesn't crash
     * the flex lexer.
     */

    while ((cnt = get_decoded_line(&buff)) != 0) {

       /* Note: some malformed messages can cause xfgetsl() to report
       ** "Invalid buffer size, exiting."  ** and then abort.  This
       ** can happen when the parser is in html mode and there's a
       ** leading '<' but no closing '>'.
       **
       ** The "fix" is to check for a nearly full lexer buffer and
       ** discard most of it.
       */
       bool nearly_full = used > 1000 && used > size * 10;

       count += cnt;

       if ((count <= (MAXTOKENLEN * 3 / 2)) || not_long_token(buff.t.text, (uint) count))
	   if (!nearly_full)
	       break;

       if (count >= MAXTOKENLEN * 2) {
	   size_t shift = count - MAXTOKENLEN;
	   memmove(buff.t.text, buff.t.text + shift, MAXTOKENLEN+D);
	   count = MAXTOKENLEN;
	   buff.t.leng = MAXTOKENLEN;
       }

       if (nearly_full)
	   break;
    }

//extern mime_t *msg_state;
  if(msg_state)
  {  if(msg_state->mime_disposition)
     {  if(msg_state->mime_type == MIME_APPLICATION ||  msg_state->mime_type == MIME_IMAGE)
	 return (count == EOF ? 0 : count);   //not decode at all
     }
  }

//EK decoding things like &#1084 and charset_table;
#ifdef	CP866
    count = htmlUNICODE_decode(buf, count);
#else
    for (i = 0; i < count; i++ )
    {
	byte ch = buf[i];
	buf[i] = charset_table[ch];
    }
#endif
    return (count == EOF ? 0 : count);
}

size_t text_decode(word_t *w)
{
    byte *const beg = w->text;		/* base pointer, fixed */
    byte *const fin = beg + w->leng;	/* end+1 position */

    byte *txt = (byte *) memstr(w->text, w->leng, "=?");	/* input position */
    uint size = (uint) (txt - beg);				/* output offset */

    if (txt == NULL)
       return w->leng;

    while (txt < fin) {
       word_t n;
       byte *typ, *tmp, *end;
       uint len;
       bool adjacent;

       typ = (byte *) memchr((char *)txt+2, '?', fin-(txt+2));	/* Encoding type - 'B' or 'Q' */
       tmp = typ + 3;						/* start of encoded word */
       end = (byte *) memstr((char *)tmp, fin-tmp, "?=");	/* last byte of encoded word  */
       len = end - tmp;

       n.text = tmp;				/* Start of encoded word */
       n.leng = len;				/* Length of encoded word */

       if (DEBUG_LEXER(2)) {
           fputs("***  ", dbgout);
           word_puts(&n, 0, dbgout);
           fputs("\n", dbgout);
       }

       switch (tolower(typ[1])) {		/* ... encoding type */
       case 'b':
           if (base64_validate(&n))
               len = base64_decode(&n);		/* decode base64 */
           break;
       case 'q':
           if (qp_validate(&n, RFC2047))
               len = qp_decode(&n, RFC2047);	/* decode quoted-printable */
           break;
       }

       n.leng = len;

       if (DEBUG_LEXER(3)) {
           fputs("***  ", dbgout);
           word_puts(&n, 0, dbgout);
           fputs("\n", dbgout);
       }

       /* move decoded word to where the encoded used to be */
       memmove(beg+size, n.text, len+1);
       size += len;	/* bump output pointer */
       txt = end + 2;	/* skip ?= trailer */
       if (txt >= fin)
           break;

       /* check for next encoded word */
       end = (byte *) memstr((char *)txt, fin-txt, "=?");
       adjacent = end != NULL;

       /* clear adjacent flag if non-whitespace character found between
        * adjacent encoded words */
       if (adjacent) {
           tmp = txt;
           while (adjacent && tmp < end) {
               if (isspace(*tmp))
                   tmp += 1;
               else
                   adjacent = false;
           }
       }

       /* we have a next encoded word and we've had only whitespace
        * between the current and the next */
       if (adjacent)
           /* just skip whitespace */
           txt = end;
       else
           /* copy everything that was between the encoded words */
           while (txt < end)
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
