/* $Id$ */

/*****************************************************************************

NAME:
   token.c -- post-lexer token processing

   12/08/02 - split out from lexer.l

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#include "common.h"

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

/* Local Variables */

word_t *yylval = NULL;
word_t *msg_addr = NULL;	/* First IP Address in Received: statement */

static token_t save_class = NONE;
static word_t *ipsave = NULL;

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
static word_t *nonblank_line = NULL;

#define WFREE(n)  word_free(n); n = NULL/* Global Variables */

/* Function Prototypes */

/* Function Definitions */

token_t get_token(void)
{
    token_t cls = NONE;
    unsigned char *cp;
    bool done = false;

    /* If saved IPADDR, truncate last octet */
    if ( block_on_subnets && save_class == IPADDR )
    {
	byte *t = xmemrchr(ipsave->text, '.', ipsave->leng);
	if (t == NULL)
	    save_class = NONE;
	else
	{
	    *t = (byte) '\0';
	    ipsave->leng = (uint) (t - ipsave->text);
	    yylval = ipsave;
	    return save_class;
	}
    }

    if (yylval == NULL)
	yylval = word_new(NULL, 0);

    while (!done) {
	cls = (*lexer->yylex)();
	yylval->leng = (uint) *lexer->yyleng;
	yylval->text = (unsigned char *)(*lexer->yytext);

	if (DEBUG_TEXT(2)) {
	    word_puts(yylval, 0, dbgout);
	    fputc('\n', dbgout);
	}

	if (cls == NONE) /* End of message */
	    break;

	switch (cls) {

	case EOH:	/* end of header - bogus if not empty */
	    if (msg_state->mime_type == MIME_MESSAGE)
		mime_add_child(msg_state);
	    if (yylval->leng == 2)
		continue;
	    else	/* "spc:invalid_end_of_header" */
		yylval = word_dup(nonblank_line);
	    break;

	case BOUNDARY:	/* don't return boundary tokens to the user */
	    continue;

	case VERP:	/* Variable Envelope Return Path */
	{
	    byte *st = (byte *)yylval->text;
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
		yylval->leng = (uint) (ot - st);
		Z(yylval->text[yylval->leng]);	/* for easier debugging - removable */
	    }
	    if (token_prefix != NULL) {
		word_t *o = yylval;
		yylval = word_concat(token_prefix, yylval);
		word_free(o);
	    }
	}
	break;

	case HEADKEY:
	{
	    if (!header_line_markup)
		continue;
	    else {
		const char *delim = strchr((const char *)yylval->text, ':');
		yylval->leng = (uint) (delim - (const char *)yylval->text);
		Z(yylval->text[yylval->leng]);	/* for easier debugging - removable */
	    }
	}

	/*@fallthrough@*/

	case TOKEN:	/* ignore anything when not reading text MIME types */
	    if (token_prefix != NULL) {
		word_t *o = yylval;
		yylval = word_concat(token_prefix, yylval);
		word_free(o);
	    }
	    else {
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

	case MSGADDR:
	    /* trim brackets */
	    yylval->leng -= 2;
	    memcpy(yylval->text, yylval->text+1, yylval->leng);
	    Z(yylval->text[yylval->leng]);	/* for easier debugging - removable */
	    /* if top level, no address, not localhost, .... */
	    if (token_prefix == w_recv &&
		msg_state == msg_state->parent && 
		msg_addr == NULL &&
		strcmp((char *)yylval->text, "127.0.0.1") != 0) {
		/* Not guaranteed to be the originating address of the message. */
		word_free(msg_addr);
		msg_addr = word_dup(yylval);
	    }
	case IPADDR:
	    if (block_on_subnets)
	    {
		const byte *prefix = (wordlist_version >= IP_PREFIX) ? (const byte *)"ip:" : (const byte *)"url:";
		size_t plen = strlen((const char *)prefix);
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
		if (sscanf((const char *)yylval->text, "%d.%d.%d.%d", &q1, &q2, &q3, &q4) == 4)
		    /* safe because result string guaranteed to be shorter */
		    sprintf((char *)yylval->text, "%d.%d.%d.%d",
			    q1 & 0xff, q2 & 0xff, q3 & 0xff, q4 & 0xff);
		yylval->leng = strlen((const char *)yylval->text);
		ipsave = word_new(NULL, plen + yylval->leng);
		memcpy(ipsave->text, prefix, plen);
		memcpy(ipsave->text+plen, yylval->text, yylval->leng+1);
		word_free(yylval);
		yylval = ipsave;
		save_class = IPADDR;
		return (cls);
	    }
	    if (token_prefix != NULL) {
		word_t *o = yylval;
		yylval = word_concat(token_prefix, yylval);
		word_free(o);
	    }
	    break;

	case NONE:		/* nothing to do */
	    break;

	case MSG_COUNT_LINE:
	    msg_count_file = true;
	    header_line_markup = false;
	    token_prefix = NULL;
	    lexer = &msg_count_lexer;
	    reader_more = msgcount_more;
	    continue;

	case BOGO_LEX_LINE:
	    done = true;
	    break;
	}

	if (DEBUG_TEXT(1)) {
	    word_puts(yylval, 0, dbgout);
	    fputc('\n', dbgout);
	}

	/* eat all long words */
	if (yylval->leng <= MAXTOKENLEN)
	    done = true;
    }

    if (!msg_count_file) {
	/* Remove trailing blanks */
	/* From "From ", for example */
	while (yylval->leng > 1 && yylval->text[yylval->leng-1] == ' ') {
	    yylval->leng -= 1;
	    yylval->text[yylval->leng] = (byte) '\0';
	}

	/* Remove trailing colon */
	if (yylval->leng > 1 && yylval->text[yylval->leng-1] == ':') {
	    yylval->leng -= 1;
	    yylval->text[yylval->leng] = (byte) '\0';
	}

	/* Need separate loop so lexer can see "From", "Date", etc *
	 * depending on options set, replace nonascii characters by '?'s
	 * and/or replace upper case by lower case
	 */
	for (cp = yylval->text; cp < yylval->text+yylval->leng; cp += 1)
	    *cp = casefold_table[*cp];
    }

    return(cls);
}

void token_init(void)
{
    yyinit();

    if (!msg_count_file)
	mime_reset();

    if (nonblank_line == NULL) {
	const char *s = "spc:invalid_end_of_header";
	nonblank_line = word_new((const byte *)s, strlen(s));
    }

    if (w_to == NULL) {
	w_to   = word_new((const byte *) "to:",   0);	/* To:          */
	w_from = word_new((const byte *) "from:", 0);	/* From:        */
	w_rtrn = word_new((const byte *) "rtrn:", 0);	/* Return-Path: */
	w_subj = word_new((const byte *) "subj:", 0);	/* Subject:     */
	w_recv = word_new((const byte *) "rcvd:", 0);	/* Received:    */
	w_head = word_new((const byte *) "head:", 0);	/* Header:      */
	w_mime = word_new((const byte *) "mime:", 0);	/* Mime:        */
    }

    WFREE(msg_addr);

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

    if (msg_state != msg_state->parent &&
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
	if (msg_state == msg_state->parent)
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
    return;
}

/* Cleanup storage allocation */
void token_cleanup()
{
    WFREE(yylval);
    WFREE(nonblank_line);
    WFREE(w_to);
    WFREE(w_from);
    WFREE(w_rtrn);
    WFREE(w_subj);
    WFREE(w_recv);
    WFREE(w_head);
    WFREE(w_mime);
    WFREE(msg_addr);
}
