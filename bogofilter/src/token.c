/* $Id$ */

/*****************************************************************************

NAME:
   token.c -- post-lexer token processing

   12/08/02 - split out from lexer.l

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#include <stdlib.h>

#include <config.h>
#include "common.h"

#include "charset.h"
#include "error.h"
#include "mime.h"
#include "word.h"
#include "token.h"
#include "xmemrchr.h"

/* Structure Definitions */

typedef enum {
    LEXER_HEAD,
    LEXER_TEXT,
    LEXER_HTML,
} lexer_state_t;

/* Local Static Variables */

word_t *yylval = NULL;

static token_t save_class = NONE;
static word_t *bfsave = NULL;

static int html_tag_level = 0;
static int html_comment_level = 0;

static lexer_state_t lexer_state = LEXER_HEAD;

/* Global Variables */

bool block_on_subnets = false;

/* Function Prototypes */

static void reset_html_level(void);
static void change_lexer_state(lexer_state_t new);
static const char *state_name(lexer_state_t new);

/* Function Definitions */

static
const char *state_name(lexer_state_t state)
{
    switch(state) {
    case LEXER_HEAD: return "HEAD";
    case LEXER_TEXT: return "TEXT";
    case LEXER_HTML: return "HTML";
    }
    return "unknown";
}

static
void change_lexer_state(lexer_state_t new)
{
    /* if change of state, show new state */
    if (DEBUG_LEXER(1) && lexer_state != new)
	fprintf(dbgout, "lexer_state: %s -> %s\n", state_name(lexer_state), state_name(new));
    lexer_state = new;
    return;
}

static
void reset_html_level(void)
{
    html_tag_level = 0;
    html_comment_level = 0;
}

void html_tag(int level)
{
    html_tag_level = level;
}

void html_comment(int level)
{
    html_comment_level += level;
    if (html_comment_level < 0)
	html_comment_level = 0;
}

token_t get_token(void)
{
    token_t class = NONE;
    unsigned char *cp;

    /* If saved IPADDR, truncate last octet */
    if ( block_on_subnets && save_class == IPADDR )
    {
	byte *t = xmemrchr(bfsave->text, '.', bfsave->leng);
	if (t == NULL)
	    save_class = NONE;
	else
	{
	    *t = '\0';	
	    bfsave->leng = t - bfsave->text;
	    yylval = bfsave;
	    return save_class;
	}
    }

    if (yylval == NULL)
	yylval = word_new(NULL, 0);

    while (true) {
	switch (lexer_state) {
	case LEXER_HEAD: 
 	    class  = lexer_lex();
	    yylval->leng = lexer_leng;
	    yylval->text = (byte *)lexer_text;
	    break;
	case LEXER_TEXT: 
	    class  = text_plain_lex();
	    yylval->leng = text_plain_leng;
	    yylval->text = (byte *)text_plain_text;
	    break;
	case LEXER_HTML: 
	    class  = text_html_lex();
	    yylval->leng = text_html_leng;
	    yylval->text = (byte *)text_html_text;
	    break;
	}

	if (class <= 0)
	    break;

	switch (class) {

	case EMPTY:	/* empty line -- ignore */
	    continue;

	case BOUNDARY:	/* don't return boundary tokens to the user */
	    if (mime_is_boundary(yylval)){
    	    	change_lexer_state(LEXER_HEAD);
		continue;
	    }

	case TOKEN:	/* ignore anything when not reading text MIME types */
	      if (html_tag_level > 0 || html_comment_level > 0)
		continue;
	      
	    if (msg_state->mime_header)
		break;

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
		bfsave = word_new(NULL, plen + yylval->leng);
		memcpy(bfsave->text, prefix, plen);
		memcpy(bfsave->text+plen, yylval->text, yylval->leng);
		yylval = bfsave;
		save_class = IPADDR;
		return (class);
	    }
	    break;
	case NONE:		/* nothing to do */
	    break;
	case FROM:		/* nothing to do */
	    break;
	}

	/* eat all long words */
	if (yylval->leng <= MAXTOKENLEN)
	    break;
    }

    /* Remove trailing blanks */
    /* From "From ", for example */
    while (yylval->leng > 1 && yylval->text[yylval->leng-1] == ' ') {
	yylval->leng -= 1;
	yylval->text[yylval->leng] = '\0';
    }

    /* Need separate loop so lexer can see "From", "Date", etc */
    for (cp = yylval->text; cp < yylval->text+yylval->leng; cp += 1)
	*cp = casefold_table[*cp];

    return(class);
}

void got_from(void)
{
    change_lexer_state(LEXER_HEAD);
    mime_reset(); 
    reset_html_level();
}

void got_newline()
{

    if (msg_state->mime_type != MIME_MESSAGE && !msg_state->mime_header)
	return;

    msg_header = msg_state->mime_header = false;
     
    switch (msg_state->mime_type) {

    case LEXER_HEAD:
	change_lexer_state(LEXER_TEXT);
	break;

    case MIME_TEXT:
	change_lexer_state(LEXER_TEXT);
	break;

    case MIME_TEXT_PLAIN:
	change_lexer_state(LEXER_TEXT);
	break;

    case MIME_TEXT_HTML:
	change_lexer_state(LEXER_HTML);
	break;

    case MIME_MESSAGE:
      mime_add_child(msg_state);
      change_lexer_state(LEXER_HEAD);

    default:
	change_lexer_state(LEXER_TEXT);
    }

    return;
}
