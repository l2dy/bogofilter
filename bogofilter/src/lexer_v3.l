/* $Id$ */

%{
/*
 * NAME
 *   lexer_header.l -- bogofilter's lexical analyzer for message headers
 *
 *   01/01/2003 - split out of lexer.l
 *
*/

/*
 * Our lexical analysis is different from Paul Graham's rules: 
 *
 * We throw away headers that are readily identifiable as dates.
 * We throw away text lines that look like BASE64 encoding.
 * We throw away all digit strings that don't look like IP address parts.
 * We thow away lines beginning with <tab>id<space> -- mailer UDs.
 * We throw away various bits of MIME cruft (things that look like
 * declarations and boundaries for multipart declarations).
 * We throw away *all* tokens of length 1 or 2.
 *
 * These are optimizations to keep the token lists from bloating.
 * The big win is recognizing machine-generated unique IDs that
 * we'll never see again and shouldn't 
 *
 * We don't treat dot between two alphanumerics as a separator,
 * because we want to keep domain names and IP addresses together as 
 * recognizable units. 
 *
 * Having done the above, there isn't much need to recognize URLs.  
 * If a URL is a spam indicator, very likely any other URL from the
 * same site is as well, so the hostname part should be an adequate
 * statistical trigger.  
 *
 * The list of HTML keywords and attributes to be ignored is from the 4.0 DTD.
 *
 * LEXED_TOKENS, which are found in "msg-count" files need a special pattern
 * because they can be:
 *	1 - normal bogofilter tokens
 *	2 - url:xxx and subj: tokens
 *	3 - mime boundaries
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include <config.h>
#include "common.h"

#include "buff.h"
#include "charset.h"
#include "lexer.h"
#include "mime.h"		/* for mime_*() */
#include "msgcounts.h"
#include "textblock.h"
#include "token.h"

#define YY_DECL token_t yylex(void)
    YY_DECL;			/* declare function */

#define YY_INPUT(buf,result,max_size) result = yyinput((byte *)buf, max_size)

static word_t yyt;
static word_t *yy_text(void);
static int lineno;

static word_t *yy_text(void)
{
    yyt.text = (byte *)yytext;
    yyt.leng = yyleng;
    return &yyt;
}

%}

%option debug nodebug
%option align nounput noyywrap noreject 8bit caseless
%option prefix="lexer_v3_"

UINT8		([01]?[0-9]?[0-9]|2([0-4][0-9]|5[0-5]))
IPADDR		{UINT8}\.{UINT8}\.{UINT8}\.{UINT8}
MIME_BOUNDARY	[0-9a-zA-Z\'()+_,-./:=?#]{1,70}

ID		[0-9a-zA-Z-]*
MTYPE		[ \t]*[0-9a-zA-Z/-]*

NUM_NUM		\ [0-9]+\ [0-9]+
MSG_COUNT	^\"\.MSG_COUNT\"

TOKEN_ESR	[A-Za-z$][A-Za-z0-9$\'.-]+[A-Za-z0-9$]

TOKENMID_CA_075	[^][:blank:]<>;=():&%$#@!+|/\\{}^\"\?\*,[:cntrl:][]+
TOKENMID_NJS	[^\][:blank:]<>;=():&%$#@!+|/\\{}^\"\?\*,[:cntrl:]\[]+
TOKENMID_NJS_DR	[^[:blank:]<>;=():&%$#@!+|/\\{}^\"\?\*,[:cntrl:]\[\]]+

TOKENFRONT	[^[:blank:][:cntrl:][:digit:][:punct:]]
TOKENMID	[^[:blank:]<>;=():&%$#@!+|/\\{}^\"\?\*,[:cntrl:]\[\]]+
BOGOLEX_TOKEN	[^[:blank:]<>;    &%$ @! |/\\{}^\"  \*,[:cntrl:]\[\]]+
TOKENBACK	[^[:blank:][:punct:][:cntrl:]]

TOKEN 		{TOKENFRONT}{TOKENMID_NJS_DR}{TOKENBACK}

T2		{TOKENFRONT}<>;=():&%$#@!+|/\\{}^\"\?\*,[:cntrl:]\[]+
T1		{TOKENFRONT}

%s TEXT HTML BOGO_LEX

%%

^From\ 						{ if (msg_header && is_from(yy_text())) { 
						      BEGIN(INITIAL); return FROM;
						  } else { 
						      return TOKEN;
						  }
						}

<INITIAL,BOGO_LEX>{MSG_COUNT}{NUM_NUM}		{ if (lineno == 0) { BEGIN(BOGO_LEX); } 
						  return MSG_COUNT_LINE;
						}
<BOGO_LEX>^\"{BOGOLEX_TOKEN}\"{NUM_NUM}		{ return BOGO_LEX_LINE; }
<BOGO_LEX>\n					{ lineno += 1; }

<INITIAL>charset=\"?{ID}\"?			{ got_charset(yytext); yyredo(yy_text(), '='); }
<INITIAL>^MIME-Version:.*			{ mime_version(yy_text()); yyless(strlen("MIME-Version:")); return(TOKEN); }
<INITIAL>^Content-Transfer-Encoding:{MTYPE}	{ mime_encoding(yy_text()); yyless(strlen("Content-Transfer-Encoding:")); return(TOKEN); }
<INITIAL>^Content-Type:{MTYPE};?		{ mime_type(yy_text()); yyredo(yy_text(), ':'); }
<INITIAL>^Content-Disposition:{MTYPE}		{ mime_disposition(yy_text()); yyredo(yy_text(), ':'); }
<INITIAL>^Subject:.* 				{ set_tag("subj:");  yyredo(yy_text(), ':'); }
<INITIAL>^Date:.*|Delivery-Date:.*		;
<INITIAL>^Message-ID:.*				;
<INITIAL>^\tid\ {ID}				;

<INITIAL>E?SMTP\ id\ {ID}			;
<INITIAL>boundary=[ ]*\"?{MIME_BOUNDARY}\"?	{ mime_boundary_set(yy_text()); }
<INITIAL>name=\"?				;
<INITIAL>filename=\"?				;

^--{MIME_BOUNDARY}(--)?$			{ if (got_mime_boundary(yy_text())) {
						      BEGIN(INITIAL); 
						      return BOUNDARY; 
						  }
						  else { 
						      return TOKEN;
						  }
						}
{IPADDR}					{ return(IPADDR);}
{TOKEN}						{ return(TOKEN);}

<HTML>\<\!--					{ html_comment(+1); }
<HTML>-->					{ html_comment(-1); }
<HTML>\<					{ html_tag(1); }
<HTML>\>					{ html_tag(0); }

.						;
^\n						{ got_emptyline(); 
						  lineno += 1;
						  switch (get_content_type()) 
						  {
						    case MIME_TEXT_PLAIN: BEGIN(TEXT); break;
						    case MIME_TEXT_HTML:  BEGIN(HTML); break;
						    default:              BEGIN(TEXT); break;
						  }
						  return(EMPTY);
						}
\n						{ got_newline(); 
						  lineno += 1;
						}

%%

void lexer_v3_init(FILE *fp)
{
    lineno = 0;
    BEGIN(INITIAL);
    yyrestart(fp);
}

/*
 * The following sets edit modes for GNU EMACS
 * Local Variables:
 * mode:c
 * End:
 */
