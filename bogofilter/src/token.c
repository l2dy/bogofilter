/* $Id$ */

/*****************************************************************************

NAME:
   token.c -- post-lexer token processing

   12/08/02 - split out from lexer.l

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#include <ctype.h>
#include <stdlib.h>

#include <config.h>
#include "common.h"

#include "base64.h"
#include "charset.h"
#include "error.h"
#include "mime.h"
#include "qp.h"
#include "word.h"
#include "token.h"
#include "xmemrchr.h"

/* Local Variables */

word_t *yylval = NULL;

static token_t save_class = NONE;
static word_t *ipsave = NULL;

/* Global Variables */

bool block_on_subnets = false;

static word_t *token_prefix = NULL;
static word_t *nonblank_line = NULL;

/* Function Definitions */

token_t get_token(void)
{
    token_t class = NONE;
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
	class = lexer->yylex();
	yylval->leng = *lexer->yyleng;
	yylval->text = (unsigned char *)(*lexer->yytext);

	if (DEBUG_TEXT(2)) { 
	    word_puts(yylval, 0, dbgout);
	    fputc('\n', dbgout);
	}
	    
	if (class <= 0)
	    break;

	switch (class) {

	case EMPTY:	/* empty line -- check for bogus end of header */
	    if (yylval->leng == 1) 
		continue;
	    else
		yylval = word_dup(nonblank_line);
	    break;

	case BOUNDARY:	/* don't return boundary tokens to the user */
	    if (mime_is_boundary(yylval))
		continue;

	case TOKEN:	/* ignore anything when not reading text MIME types */
	    if (msg_header)
	    {
		if (token_prefix != NULL) {
		    word_t *w = word_concat(token_prefix, yylval);
		    word_free(yylval);
		    yylval = w;
		}
		break;
	    }

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
		return (class);
	    }
	    break;
	case NONE:		/* nothing to do */
	    break;
	case MSG_COUNT_LINE:
	    msg_count_file = true;
	    lexer = &msg_count_lexer;
	case BOGO_LEX_LINE:
	    done = true;
	    break;
	case FROM:		/* nothing to do */
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

    return(class);
}

void token_init(void)
{
    msg_header = true;
    yyinit();
    mime_reset(); 
    if (nonblank_line == NULL) {
	const char *s = "spc:invalid_end_of_header";
	nonblank_line = word_new((const unsigned char *)s, strlen(s));
    }
}

size_t decode_text(word_t *w)
{
    size_t size = w->leng;
    char *text = (char *) w->text;
    char *beg = strchr(text, '=');
    char *enc = strchr(beg+2,  '?');
    word_t n;
    n.text = (unsigned char *)(enc + 3);
    n.leng = size - (enc+3 - text + 2);
    n.text[n.leng] = '\0';

    switch (tolower(enc[1])) {
    case 'b':
	size = base64_decode(&n);
	break;
    case 'q':
	size = qp_decode(&n);
	break;
    }

    memcpy(beg, n.text, size);
    size += beg-text;
    beg[size] = '\0';
    return size;
}

void got_from(void)
{
    token_init();
}

void got_newline()
{
    word_free(token_prefix);
    token_prefix = NULL;
}

void got_emptyline(void)
{
    if (msg_state->mime_type != MIME_MESSAGE && !msg_header)
	return;

    msg_header = false;
     
    if (msg_state->mime_type == MIME_MESSAGE)
	mime_add_child(msg_state);

    return;
}

const char *prefixes = "|to:|from:|rtrn:|subj:";

void set_tag(const char *tag)
{
    if (header_line_markup) {
	const char *tmp;
	size_t len = strlen(tag);
	
	tmp = prefixes;
	while (tmp != NULL) {
	    if (memcmp(tmp, tag, len) == 0) {
		word_free(token_prefix);
		token_prefix = word_new((const byte *)tag, strlen(tag));
		return;
	    }
	    tmp = strchr(tmp, '|');
	    if (tmp) tmp++;
	}
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
