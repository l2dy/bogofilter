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

static token_t save_class = NONE;
static word_t *ipsave = NULL;

static word_t *w_to   = NULL;	/* To: */
static word_t *w_from = NULL;	/* From: */
static word_t *w_rtrn = NULL;	/* Return-Path: */
static word_t *w_subj = NULL;	/* Subject: */

/* Global Variables */

bool block_on_subnets = false;

static word_t *token_prefix = NULL;
static word_t *nonblank_line = NULL;

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
	    *t = '\0';	
	    ipsave->leng = t - ipsave->text;
	    yylval = ipsave;
	    return save_class;
	}
    }

    if (yylval == NULL)
	yylval = word_new(NULL, 0);

    while (!done) {
	cls = lexer->yylex();
	yylval->leng = *lexer->yyleng;
	yylval->text = (unsigned char *)(*lexer->yytext);

	if (DEBUG_TEXT(2)) { 
	    word_puts(yylval, 0, dbgout);
	    fputc('\n', dbgout);
	}
	    
	if (cls == NONE)
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

	case HEADKEY:
	{
	    const char *delim = index((const char *)yylval->text, ':');
	    yylval->leng = delim - (const char *)yylval->text;
	    Z(yylval->text[yylval->leng]);
	}

	/*@fallthrough@*/

	case TOKEN:	/* ignore anything when not reading text MIME types */
	    if (token_prefix != NULL) {
		word_t *w = word_concat(token_prefix, yylval);
		word_free(yylval);
		yylval = w;
	    }
	    else {
		switch (msg_state->mime_type) {
		case MIME_TEXT:
		case MIME_TEXT_HTML:
		case MIME_TEXT_PLAIN:
		case MIME_MULTIPART:
		case MIME_MESSAGE:
		    break;
		default:
		    continue;
		}
	    }
	    break;

	case IPADDR:
	    if (block_on_subnets)
	    {
		const byte *prefix = (const byte *)"url:";
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
	    break;
	case NONE:		/* nothing to do */
	    break;
	case MSG_COUNT_LINE:
	    msg_count_file = true;
	    lexer = &msg_count_lexer;
	    reader_more = msgcount_more;
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
	    yylval->text[yylval->leng] = '\0';
	}

	/* Remove trailing colon */
	if (yylval->leng > 1 && yylval->text[yylval->leng-1] == ':') {
	    yylval->leng -= 1;
	    yylval->text[yylval->leng] = '\0';
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
    mime_reset(); 
    if (nonblank_line == NULL) {
	const char *s = "spc:invalid_end_of_header";
	nonblank_line = word_new((const byte *)s, strlen(s));
    }

    if (w_to == NULL) {
	w_to   = word_new((const byte *) "to:",   0);	/* To: */
	w_from = word_new((const byte *) "from:", 0);	/* From: */
	w_rtrn = word_new((const byte *) "rtrn:", 0);	/* Return-Path: */
	w_subj = word_new((const byte *) "subj:", 0);	/* Subject: */
    }

    return;
}

void clr_tag(void)
{
    token_prefix = NULL;
}

void set_tag(word_t *text)
{
    switch (tolower(*text->text)) {
    case 't':			/* To: */
	token_prefix = w_to;
	break;
    case 'f':			/* From: */
	token_prefix = w_from;
	break;
    case 'r':			/* Return-Path: */
	token_prefix = w_rtrn;
	break;
    case 's':			/* Subject: */
	token_prefix = w_subj;
	break;
    default:
	fprintf(stderr, "%s:%d  invalid tag - '%s'\n", 
		__FILE__, __LINE__, 
		(char *)text->text);
	exit(EX_ERROR);
    }
    return;
}

/* Cleanup storage allocation */
void token_cleanup()
{
    if (yylval)
	word_free(yylval);
    yylval = NULL;
    if (nonblank_line)
	word_free(nonblank_line);
    nonblank_line = NULL;
}
