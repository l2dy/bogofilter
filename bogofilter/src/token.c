/* $Id$ */

/*****************************************************************************

NAME:
   token.c -- post-lexer token processing

   12/08/02 - split out from lexer.l

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#include "common.h"

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>

#include "bogoreader.h"
#include "charset.h"
#include "error.h"
#include "mime.h"
#include "msgcounts.h"
#include "word.h"
#include "token.h"
#include "xmemrchr.h"

#define	MAX_PREFIX_LEN 	  5		/* maximum length of prefix     */
#define	MSG_COUNT_PADDING 2 * 10	/* space for 2 10-digit numbers */

/* Local Variables */

byte	msg_addr_text[MAXTOKENLEN + D];
byte	msg_id_text  [MAXTOKENLEN * 3 + D];
byte	queue_id_text[MAXTOKENLEN + D];
byte	ipsave_text[MAXTOKENLEN + D];

word_t	msg_addr = { 0, msg_addr_text};	/* First IP Address in Received: statement */
word_t	msg_id   = { 0, msg_id_text};	/* Message ID */
word_t	queue_id = { 0, queue_id_text};	/* Message's first queue ID */

static token_t save_class = NONE;
static word_t ipsave = { 0, ipsave_text};

byte yylval_text[MAXTOKENLEN + MAX_PREFIX_LEN + MSG_COUNT_PADDING + D];
static word_t yylval = { 0, yylval_text };

static word_t *w_to   = NULL;	/* To:          */
static word_t *w_from = NULL;	/* From:        */
static word_t *w_rtrn = NULL;	/* Return-Path: */
static word_t *w_subj = NULL;	/* Subject:     */
static word_t *w_recv = NULL;	/* Received:    */
static word_t *w_head = NULL;	/* Header:      */
static word_t *w_mime = NULL;	/* Mime:        */

/* Global Variables */

bool block_on_subnets = false;

static word_t *token_prefix = NULL;
static uint32_t token_prefix_len;

#define NONBLANK "spc:invalid_end_of_header"
static word_t *nonblank_line = NULL;

/* Function Prototypes */

void token_clear(void);

/* Function Definitions */

static void token_set( word_t *token, byte *text, uint leng )
{
    token->leng = leng;
    memcpy(token->text, text, leng);		/* include nul terminator */
    token->text[leng] = '\0';			/* ensure nul termination */
}

static inline void token_copy( word_t *dst, word_t *src )
{
    token_set(dst, src->text, src->leng);
}

static void build_prefixed_token( word_t *token, uint32_t token_size,
				  word_t *prefix, 
				  byte *text, uint32_t leng )
{
    uint p = (prefix == NULL) ? 0 : prefix->leng;
    uint l = leng + p;

    if (l >= token_size)
	l = token_size - p - 1;

    token->leng = l;

    /* copy prefix, if there is one */
    if (prefix != NULL)
	memcpy(token->text, prefix->text, p);

    memcpy(token->text + p, text, l-p);		/* include nul terminator */
    token->text[token->leng] = '\0';		/* ensure nul termination */
}

token_t get_token(word_t **token)
{
    token_t cls = NONE;
    unsigned char *cp;
    bool done = false;

    /* If saved IPADDR, truncate last octet */
    if ( block_on_subnets && save_class == IPADDR )
    {
	byte *t = xmemrchr(ipsave.text, '.', ipsave.leng);
	if (t == NULL)
	    save_class = NONE;
	else
	{
	    ipsave.leng = (uint) (t - ipsave.text);
	    token_set( &yylval, ipsave.text, ipsave.leng);
	    cls = save_class;
	    done = true;
	}
    }

    while (!done) {
	uint leng;
	byte *text;
	cls = (*lexer->yylex)();
	
	leng = (uint)   *lexer->yyleng;
	text = (byte *) *lexer->yytext;
	
	if (DEBUG_TEXT(2)) {
	    word_puts(&yylval, 0, dbgout);
	    fputc('\n', dbgout);
	}
 
	if (cls == NONE) /* End of message */
	    break;

	switch (cls) {

	case EOH:	/* end of header - bogus if not empty */
	    if (leng > MAXTOKENLEN)
		continue;

	    if (msg_state->mime_type == MIME_MESSAGE)
		mime_add_child(msg_state);
	    if (leng == 2)
		continue;
	    else {	/* "spc:invalid_end_of_header" */
		token_copy( &yylval, nonblank_line);
		done = true;
	    }
	    break;

	case BOUNDARY:	/* don't return boundary tokens to the user */
	    continue;

	case VERP:	/* Variable Envelope Return Path */
	{
	    byte *st = (byte *)text;
	    byte *in;
	    byte *fst = NULL;
	    byte *lst = NULL;

	    for (in = st; *in != '\0'; in += 1) {
		if (*in == '-') {
		    if (fst == NULL)
			fst = in;
		    lst = in;
		}
	    }
	    if (fst != NULL && lst != NULL && lst - fst  > 3) {
		byte *ot = fst;
		*ot++ = '-';
		*ot++ = '#';
		for (in = lst; *in != '\0'; in += 1, ot += 1)
		    *ot = *in;
		leng = (uint) (ot - st);
	    }
	    text[leng] = '\0';		/* ensure nul termination */
	    build_prefixed_token(&yylval, sizeof(yylval_text), token_prefix, text, leng);
	}
	break;

	case HEADKEY:
	{
	    if (!header_line_markup)
		continue;
	    else {
		const char *delim = strchr((const char *)text, ':');
		leng = (uint) (delim - (const char *)text);
		if (leng > MAXTOKENLEN)
		    continue;
		token_set( &yylval, text, leng);
	    }
	}

	/*@fallthrough@*/

	case TOKEN:	/* ignore anything when not reading text MIME types */
	    if (leng > MAXTOKENLEN)
		continue;
	    build_prefixed_token(&yylval, sizeof(yylval_text), token_prefix, text, leng);
	    if (token_prefix == NULL) {
		switch (msg_state->mime_type) {
		case MIME_TEXT:
		case MIME_TEXT_HTML:
		case MIME_TEXT_PLAIN:
		case MIME_MULTIPART:
		    break;
		case MIME_MESSAGE:
		case MIME_APPLICATION:
		case MIME_IMAGE:
		default:
		    continue;
		}
	    }
	    break;

	case MESSAGE_ID:
	    /* special token;  saved for formatted output, but not returned to bogofilter */
	    /** \bug: the parser MUST be aligned with lexer_v3.l! */
	    if (leng < sizeof(msg_id_text))
	    {
		while (!isspace(text[0])) {
		    text += 1;
		    leng -= 1;
		}
		while (isspace(text[0])) {
		    text += 1;
		    leng -= 1;
		}
		token_set( &yylval, text, leng);
		token_copy( &msg_id, &yylval );
	    }
	continue;

	case QUEUE_ID:
	    /* special token;  saved for formatted output, but not returned to bogofilter */
	    /** \bug: the parser MUST be aligned with lexer_v3.l! */
	    if (queue_id.leng == 0 &&
		leng < sizeof(queue_id_text) )
	    {
		while (isspace(text[0])) {
		    text += 1;
		    leng -= 1;
		}
		if (memcmp(text, "id", 2) == 0) {
		    text += 2;
		    leng -= 2;
		}
		while (isspace(text[0])) {
		    text += 1;
		    leng -= 1;
		}
		token_set( &yylval, text, leng);
		token_copy( &queue_id, &yylval );
	    }
	    continue;

	case MESSAGE_ADDR:
	{
	    /* trim brackets */
	    text += 1;
	    leng -= 2;
	    token_set( &yylval, text, leng);
	    /* if top level, no address, not localhost, .... */
	    if (token_prefix == w_recv &&
		msg_state->parent == NULL && 
		msg_addr.leng == 0 &&
		strcmp((char *)text, "127.0.0.1") != 0) {
		/* Not guaranteed to be the originating address of the message. */
		token_copy( &msg_addr, &yylval );
	    }
	}

	/*@fallthrough@*/

	case IPADDR:
	    if (block_on_subnets)
	    {
		const char *ptext = (wordlist_version >= IP_PREFIX) ? "ip:" : "url:";
		word_t *prefix = word_news(ptext);
		int q1, q2, q3, q4;
		/*
		 * Trick collected by ESR in real time during John
		 * Graham-Cummings's talk at Paul Graham's spam conference
		 * in January 2003...  Some spammers know that people are
		 * doing recognition on spamhaus IP addresses.  They use
		 * the fact that HTML clients normally interpret IP addresses
		 * by doing a simple accumulate-and-shift algorithm; they
		 * add large random multiples of 256 to the quads to
		 * mask their origin.  Nuke the high bits to unmask the
		 * address.
		 */

		if (sscanf((const char *)text, "%d.%d.%d.%d", &q1, &q2, &q3, &q4) == 4)
		    /* safe because result string guaranteed to be shorter */
		    sprintf((char *)text, "%d.%d.%d.%d",
			    q1 & 0xff, q2 & 0xff, q3 & 0xff, q4 & 0xff);
		leng = strlen((const char *)text);

		build_prefixed_token(&ipsave, sizeof(ipsave_text), prefix, text, leng);
		token_copy( &yylval, &ipsave );
		word_free(prefix);

		save_class = IPADDR;
		*token = &yylval;
		return (cls);
	    }
	    build_prefixed_token(&yylval, sizeof(yylval_text), token_prefix, text, leng);
	    break;

	case NONE:		/* nothing to do */
	    break;

	case MSG_COUNT_LINE:
	    msg_count_file = true;
	    header_line_markup = false;
	    token_prefix = NULL;
	    lexer = &msg_count_lexer;
	    if (mbox_mode) {
		/* Allows processing multiple messages, **
		** but only a single file.              */
		reader_more = msgcount_more;
	    }
	    continue;

	case BOGO_LEX_LINE:
	    token_set( &yylval, text, leng);
	    done = true;
	    break;
	}

	if (DEBUG_TEXT(1)) {
	    word_puts(&yylval, 0, dbgout);
	    fputc('\n', dbgout);
	}

	/* eat all long words */
	if (yylval.leng <= MAXTOKENLEN)
	    done = true;
    }

   if (!msg_count_file) {
	/* Remove trailing blanks */
	/* From "From ", for example */
	while (yylval.leng > 1 && yylval.text[yylval.leng-1] == ' ') {
	    yylval.leng -= 1;
	    yylval.text[yylval.leng] = (byte) '\0';
	}

	/* Remove trailing colon */
	if (yylval.leng > 1 && yylval.text[yylval.leng-1] == ':') {
	    yylval.leng -= 1;
	    yylval.text[yylval.leng] = (byte) '\0';
	}

	if (replace_nonascii_characters) {
	    /* replace nonascii characters by '?'s */
	    for (cp = yylval.text; cp < yylval.text+yylval.leng; cp += 1)
		*cp = casefold_table[*cp];
	}
    }

    *token = &yylval;

    return(cls);
}

void token_init(void)
{
    yyinit();

    token_clear();

    if (w_to == NULL) {
	/* word_new() used to avoid compiler complaints */
	w_to   = word_news("to:");	/* To:          */
	w_from = word_news("from:");	/* From:        */
	w_rtrn = word_news("rtrn:");	/* Return-Path: */
	w_subj = word_news("subj:");	/* Subject:     */
	w_recv = word_news("rcvd:");	/* Received:    */
	w_head = word_news("head:");	/* Header:      */
	w_mime = word_news("mime:");	/* Mime:        */
	nonblank_line = word_news(NONBLANK);
    }

    return;
}

void clr_tag(void)
{
    token_prefix = NULL;
}

void set_tag(const char *text)
{
    if (!header_line_markup)
	return;

    if (msg_state->parent != NULL &&
	msg_state->parent->mime_type == MIME_MESSAGE) {
	clr_tag();			/* don't tag if inside message/rfc822 */
	return;
    }

    switch (tolower(*text)) {
    case 'c':				/* CC: */
    case 't':
	token_prefix = w_to;		/* To: */
	break;
    case 'f':
	token_prefix = w_from;		/* From: */
	break;
    case 'h':
	if (msg_state->parent == NULL)
	    token_prefix = w_head;	/* Header: */
	else
	    token_prefix = w_mime;	/* Mime:   */
	break;
    case 'r':
	if (tolower(text[2]) == 't')
	    token_prefix = w_rtrn;	/* Return-Path: */
	else
	    token_prefix = w_recv;	/* Received: */
	break;
    case 's':
	token_prefix = w_subj;		/* Subject: */
	break;
    default:
	fprintf(stderr, "%s:%d  invalid tag - '%s'\n",
		__FILE__, __LINE__,
		text);
	exit(EX_ERROR);
    }

    token_prefix_len = token_prefix->leng;
    assert(token_prefix_len <= MAX_PREFIX_LEN);

    if (DEBUG_LEXER(2)) {
	fprintf(dbgout,"--- set_tag(%s) -> prefix=", text);
	if (token_prefix)
	    word_puts(token_prefix, 0, dbgout);
	fputc('\n', dbgout);
    }
    return;
}

void set_msg_id(byte *text, uint leng)
{
    if (leng >= sizeof(msg_id_text))	/* Limit length */
	leng  = sizeof(msg_id_text) - 1;
    token_set( &msg_id, text, leng );
}

#define WFREE(n)	word_free(n); n = NULL

/* Cleanup storage allocation */
void token_cleanup()
{
    WFREE(w_to);
    WFREE(w_from);
    WFREE(w_rtrn);
    WFREE(w_subj);
    WFREE(w_recv);
    WFREE(w_head);
    WFREE(w_mime);
    WFREE(nonblank_line);

    token_clear();
}

void token_clear()
{
    msg_addr.leng = 0;
    msg_id.leng   = 0;
    queue_id.leng = 0;
}
