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

word_t	*msg_addr;	/* First IP Address in Received: statement */
word_t	*msg_id;	/* Message ID */
word_t	*queue_id;	/* Message's first queue ID */

static token_t save_class = NONE;
static word_t *ipsave;

static byte  *yylval_text;
static size_t yylval_text_size;
static word_t yylval;

#define	MAX_PREFIX_LEN	5
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

static bool fMultiWordAlloc   = false;
static uint tok_count         = 0;
static uint init_token         = 1;
static word_t *p_multi_words  = NULL;
static byte   *p_multi_buff   = NULL;
static byte   *p_multi_text   = NULL;
static word_t **w_token_array = NULL;

/* Function Prototypes */

static void token_clear(void);
static token_t get_single_token(word_t *token);
static token_t get_multi_token(word_t *token);

/* Function Definitions */

static void init_token_array(void)
{
    if (!fMultiWordAlloc) {
	uint i;
	byte *text;
	word_t *words;
	w_token_array = calloc(multi_token_count, sizeof(*w_token_array));

	p_multi_words = calloc( max_token_len+D,   sizeof(word_t));
	p_multi_text  = calloc( max_token_len+D,   multi_token_count);
	p_multi_buff  = malloc((max_token_len+D) * multi_token_count + MAX_PREFIX_LEN);

	text = p_multi_text;
	words = p_multi_words;

	for (i = 0; i < multi_token_count; i += 1) {
	    words->leng = 0;
	    words->text = text;
	    w_token_array[i] = words;
	    words += 1;
	    text += max_token_len+1+1;
	}
	fMultiWordAlloc = true;
    }
}

static void free_token_array(void)
{
    assert((tok_count == 0) == (p_multi_words == NULL));
    assert((tok_count == 0) == (w_token_array == NULL));

    tok_count = 0;

    if (fMultiWordAlloc) {
	fMultiWordAlloc = false;
	free(p_multi_words);
	free(p_multi_text );
	free(w_token_array);
    }
}

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

token_t get_token(word_t *token)
{
    return get_multi_token(token);
}

#define WRAP(n)	((n) % multi_token_count)

token_t get_multi_token(word_t *token)
{
    token_t cls;

    if (tok_count < 2 ||
	tok_count <= init_token ||
	multi_token_count <= init_token) {

	cls = get_single_token(token);

	if (multi_token_count > 1) {
	    /* save token in token array */
	    word_t *w;
	    
	    w  = w_token_array[WRAP(tok_count)];

	    w->leng = token->leng;
	    memcpy(w->text, token->text, w->leng);
	    Z(w->text[w->leng]);	/* for easier debugging - removable */

	    if (DEBUG_MULTI(1))
		fprintf(stderr, "%s:%d  %2s  %2d %2d %p %s\n", __FILE__, __LINE__,
			"", tok_count, w->leng, w->text, w->text);

	    tok_count += 1;
	    init_token = 1;
	}
    }
    else {
	int tok;

	const char *sep = "";
	uint  leng;
	byte *dest;

	leng = init_token;
	for ( tok = init_token; tok >= 0; tok -= 1 ) {
	    uint idx = tok_count - 1 - tok;
	    leng += strlen((char *) w_token_array[WRAP(idx)]->text);
	}

	token->leng = leng;
	/* Note:  must free this memory */
	token->text = dest = p_multi_buff;

	for ( tok = init_token; tok >= 0; tok -= 1 ) {
	    uint  idx = tok_count - 1 - tok;
	    uint  len = w_token_array[WRAP(idx)]->leng;
	    byte *str = w_token_array[WRAP(idx)]->text;

	    if (DEBUG_MULTI(1))
		fprintf(stderr, "%s:%d  %2d  %2d %2d %p %s\n", __FILE__, __LINE__,
			idx, tok_count, len, str, str);

	    len = strlen(sep);
	    memcpy(dest, sep, len);
	    dest += len;

	    len = strlen((char *)str);
	    memcpy(dest, str, len);
	    dest += len;

	    sep = "*";
	}

	dest = token->text;
	Z(dest[leng]);		/* for easier debugging - removable */
	init_token += 1;	/* progress to next multi-token */
    }

    return cls;
}

token_t get_single_token(word_t *token)
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
	    ipsave->leng = (uint) (t - ipsave->text);
	    token_set( &yylval, ipsave->text, ipsave->leng);
	    cls = save_class;
	    done = true;
	}
    }

    while (!done) {
	uint leng;
	byte *text;

	cls = (*lexer->yylex)();

	token->leng = (uint)   *lexer->yyleng;
	token->text = (byte *) *lexer->yytext;
	Z(token->text[token->leng]);	/* for easier debugging - removable */

	leng = token->leng;
	text = token->text;

	if (DEBUG_TEXT(2)) {
	    word_puts(token, 0, dbgout);
	    fputc('\n', dbgout);
	}
 
	if (cls == NONE) /* End of message */
	    break;

	switch (cls) {

	case EOH:	/* end of header - bogus if not empty */
	    if (leng > max_token_len)
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

	    build_prefixed_token(&yylval, yylval_text_size, token_prefix, text, leng);
	    token->leng = yylval.leng;
	    token->text = yylval.text;
	}
	break;

	case HEADKEY:
	{
	    if (!header_line_markup || *text == '\0')
		continue;
	    else {
		const char *delim = strchr((const char *)text, ':');
		leng = (uint) (delim - (const char *)text);
		if (leng > max_token_len)
		    continue;
		token_set( &yylval, text, leng);
	    }
	}

	/*@fallthrough@*/

	case TOKEN:	/* ignore anything when not reading text MIME types */
	    if (leng < min_token_len)
		continue;
	case MONEY:	/* 2 character money is OK */
	    if (leng > max_token_len)
		continue;

	    build_prefixed_token(&yylval, yylval_text_size, token_prefix, text, leng);
	    token->leng = yylval.leng;
	    token->text = yylval.text;

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
	    if (leng < max_token_len)
	    {
		while (!isspace(text[0])) {
		    text += 1;
		    leng -= 1;
		}
		while (isspace(text[0])) {
		    text += 1;
		    leng -= 1;
		}
		token_set( msg_id, text, leng);
	    }
	    continue;

	case QUEUE_ID:
	    /* special token;  saved for formatted output, but not returned to bogofilter */
	    /** \bug: the parser MUST be aligned with lexer_v3.l! */
	    if (*queue_id->text == '\0' &&
		leng < max_token_len )
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
		memcpy( queue_id->text, text, min(queue_id->leng, leng)+D );
		Z(queue_id->text[queue_id->leng]);
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
		*msg_addr->text == '\0' &&
		strcmp((char *)text, "127.0.0.1") != 0)
	    {
		/* Not guaranteed to be the originating address of the message. */
		memcpy( msg_addr->text, yylval.text, min(msg_addr->leng, yylval.leng)+D );
		Z(msg_addr->text[yylval.leng]);
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

		build_prefixed_token(ipsave, max_token_len, prefix, text, leng);

		token_copy( &yylval, ipsave );
		token->leng = yylval.leng;
		token->text = yylval.text;

		word_free(prefix);
		save_class = IPADDR;

		return (cls);
	    }

	    build_prefixed_token(&yylval, yylval_text_size, token_prefix, text, leng);
	    token->leng = yylval.leng;
	    token->text = yylval.text;

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
	if (token->leng <= max_token_len)
	    done = true;
    }

   if (!msg_count_file) {
	/* Remove trailing blanks */
	/* From "From ", for example */
	while (token->leng > 1 && token->text[token->leng-1] == ' ') {
	    token->leng -= 1;
	    token->text[token->leng] = (byte) '\0';
	}

	/* Remove trailing colon */
	if (token->leng > 1 && token->text[token->leng-1] == ':') {
	    token->leng -= 1;
	    token->text[token->leng] = (byte) '\0';
	}

	if (replace_nonascii_characters) {
	    /* replace nonascii characters by '?'s */
	    for (cp = token->text; cp < token->text+token->leng; cp += 1)
		*cp = casefold_table[*cp];
	}
    }

    return(cls);
}

void token_init(void)
{
    static bool fTokenInit = false;

    yyinit();

    if ( fTokenInit) {
	token_clear();
    }
    else {
	fTokenInit = true;
	yylval_text_size = max_token_len + MAX_PREFIX_LEN + MSG_COUNT_PADDING + D;

	yylval_text = (byte *) malloc( yylval_text_size );
	yylval.leng   = 0;
	yylval.text   = yylval_text;

	/* First IP Address in Received: statement */
	msg_addr = word_new( NULL, max_token_len );

	/* Message ID */
	msg_id = word_new( NULL, max_token_len * 3 );

	/* Message's first queue ID */
	queue_id = word_new( NULL, max_token_len );

	ipsave = word_new( NULL, max_token_len );

	/* word_new() used to avoid compiler complaints */
	w_to   = word_news("to:");	/* To:          */
	w_from = word_news("from:");	/* From:        */
	w_rtrn = word_news("rtrn:");	/* Return-Path: */
	w_subj = word_news("subj:");	/* Subject:     */
	w_recv = word_news("rcvd:");	/* Received:    */
	w_head = word_news("head:");	/* Header:      */
	w_mime = word_news("mime:");	/* Mime:        */
	nonblank_line = word_news(NONBLANK);

	/* do multi-word token initializations */
	init_token_array();
    }

    return;
}

void clr_tag(void)
{
    token_prefix = NULL;
}

void set_tag(const char *text)
{
    word_t *old_prefix = token_prefix;

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

    /* discard tokens when prefix changes */
    if (old_prefix != NULL && old_prefix != token_prefix)
	tok_count = 0;

    return;
}

void set_msg_id(byte *text, uint leng)
{
    (void) leng;		/* suppress compiler warning */
    token_set( msg_id, text, msg_id->leng );
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

    /* do multi-word token cleanup */
//    free_token_array();
}

void token_clear()
{
    if (msg_addr != NULL)
    {
	*msg_addr->text = '\0';
	*msg_id->text   = '\0';
	*queue_id->text = '\0';
    }
}
