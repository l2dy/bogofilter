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
 * We throw away all digit strings that don't look like IP address parts.
 * We thow away lines beginning with <tab>id<space> -- mailer UDs.
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
 * LEXED_TOKENS, which are found in "msg-count" files need a special pattern
 * because they can be:
 *	1 - normal bogofilter tokens
 *	2 - url:xxx and subj: tokens
 *	3 - mime boundaries
 */

/* 12 May 2003
 * Added Paul Graham's latest ideas on parsing.
 * (From http://www.paulgraham.com/better.html)
 *
 * 1. Case is preserved.
 *
 * 2. Exclamation points are constituent characters.
 *
 * 3. Periods and commas are constituents if they occur between two
 *    digits. This lets me get ip addresses and prices intact.
 *
 * 4. A price range like $20-25 yields two tokens, $20 and $25.
 *
 * 5. Tokens that occur within the To, From, Subject, and Return-Path
 *    lines, or within urls, get marked accordingly.
 *    For example. `foo' in the Subject line becomes `subj:foo'.
*/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include <config.h>
#include "common.h"

#include "buff.h"
#include "charset.h"
#include "html.h"		/* for strict_check */
#include "lexer.h"
#include "mime.h"		/* for mime_*() */
#include "msgcounts.h"
#include "textblock.h"
#include "token.h"
#include "xmalloc.h"

#define YY_DECL token_t yylex(void)
    YY_DECL;			/* declare function */

#define YY_INPUT(buf,result,max_size) result = yyinput((byte *)buf, max_size)
#define YY_EXIT_FAILURE EX_ERROR

#undef	stderr
#define	stderr	dbgout		/* for debug & -D options */

static word_t yyt;
static int lineno;

/* Function Prototypes */

static word_t *yy_text(void);
static void reorder_html(void);

/* Function Definitions */

static word_t *yy_text(void)
{
    yyt.text = (byte *)yytext;
    yyt.leng = yyleng;
    return &yyt;
}

%}

%option debug nodebug
%option align caseless 8bit
%option never-interactive noreject
%option noyywrap
%option prefix="lexer_v3_"

UINT8		([01]?[0-9]?[0-9]|2([0-4][0-9]|5[0-5]))
IPADDR		{UINT8}\.{UINT8}\.{UINT8}\.{UINT8}
BCHARSNOSPC	[0-9a-zA-Z'()+_,-./:=?#]
BCHARS		[0-9a-zA-Z'()+_,-./:=?# ]
MIME_BOUNDARY	{BCHARS}{0,69}{BCHARSNOSPC}

ID		[0-9a-zA-Z-]*
MTYPE		[ \t]*[0-9a-zA-Z/-]*

NUM_NUM		\ [0-9]+\ [0-9]+
MSG_COUNT	^\"\.MSG_COUNT\"

TOKENFRONT	[^[:blank:][:cntrl:][:digit:][:punct:]]
TOKENMID	[^[:blank:]<>;=():&%$#@+|/\\{}^\"\?\*,[:cntrl:]\[\]]+
BOGOLEX_TOKEN	[^[:blank:]<>;    &%  @ |/\\{}^\"  \*,[:cntrl:]\[\]]+
TOKENBACK	[^[:blank:]<>;=():&%$#@+|/\\{}^\"\?\*\._\-\+,\[\][:cntrl:]]

TOKEN		{TOKENFRONT}{TOKENMID}{TOKENBACK}{1,70}

BASE64		[0-9a-zA-Z/+=]+
QP		[^[:blank:]]+
ENCODED_TOKEN	{BOGOLEX_TOKEN}*=\?{ID}\?(b\?{BASE64}|q\?{QP})\?\=

DOLLARS		[0-9]+
CENTS		[0-9]+

A1		[[:alpha:]]+
A2		[[:alpha:]][[:alnum:]]+
ALPHA		[[:alpha:]][[:alnum:]]+

WHITESPACE	[ \t\n]

NOTWHITESPACE	[^ \t\n]

HTML_WI_COMMENTS	"<"[^\>]*">"

HTML_WO_COMMENTS	"<"[^!][^\>]*">"|"<>"

HTMLTOKEN		{HTML_WI_COMMENTS}

/*
 * Generally, there are some html tags that cause an "eyebreak" and some
 * that do not. For example, the "P" tag or the "BR" tag cause a break,
 * and can be interpreted in place, while, the B (bold) tag does not.
 * No close tags seem to cause a break.
 * Comments do not.  This is an attempt to make an exhaustive list of
 * tags that cause an "eyebreak". When the exit tag also causes a break,
 * we include the /?. I believe this to be a complete list of tags that
 * can cause a formatting break.
 */

HBREAK		p|br|li|h[1-6]|hr|title|table|center|dd|dt|iframe|img|input|select|td|textarea|th|\/?(div|blockquote|pre|dir|dl|fieldset|legend|form|menu|ol|ul)

PARSEHTML	"<"(a|font|img)([ \n\t][^>]*|"")">"
BREAKHTML	"<"({HBREAK}([ \n\t][^>]*|""))">"

%s TEXT HTML BOGO_LEX
%s HTOKEN HDISCARD SCOMMENT LCOMMENT HSCRIPT

%%

^From\ 						{ if (msg_header && is_from((const unsigned char *)yytext, yyleng)) { 
						      BEGIN(INITIAL); 
						      return FROM;
						  } else { 
						      return TOKEN;
						  }
						}

<INITIAL,BOGO_LEX>{MSG_COUNT}{NUM_NUM}		{ if (lineno == 0) { 
						      BEGIN(BOGO_LEX); 
						      set_msg_counts(strchr(yytext, ' ') + 1); 
						  }
						  return MSG_COUNT_LINE;
						}
<BOGO_LEX>^\"{BOGOLEX_TOKEN}\"{NUM_NUM}		{ return BOGO_LEX_LINE; }
<BOGO_LEX>\n					{ lineno += 1; }

<INITIAL>{ENCODED_TOKEN}			{ word_t *w = yy_text();
						  size_t size = decode_text(w);
						  while (size-- > 0)
						    unput(w->text[size]);
						}

<INITIAL>charset=\"?{ID}\"?			{ got_charset(yytext); yyless(strlen("charset")); return TOKEN; }
<INITIAL>^MIME-Version:.*			{ mime_version(yy_text()); yyless(strlen("MIME-Version:")); return(TOKEN); }
<INITIAL>^Content-Transfer-Encoding:{MTYPE}	{ mime_encoding(yy_text()); yyless(strlen("Content-Transfer-Encoding:")); return(TOKEN); }
<INITIAL>^Content-Type:{MTYPE};?		{ mime_type(yy_text()); yyless(strlen("Content-Type:")); return TOKEN; }
<INITIAL>^Content-Disposition:{MTYPE}		{ mime_disposition(yy_text()); yyless(strlen("Content-Disposition:")); return TOKEN; }
<INITIAL>^To/:\.*				{ if (header_line_markup) set_tag("to:"); }
<INITIAL>^From/:\.*				{ if (header_line_markup) set_tag("from:"); else return TOKEN; }
<INITIAL>^Return-Path/:\.*			{ if (header_line_markup) set_tag("rtrn:"); else return TOKEN; }
<INITIAL>^Subject/:\.*				{ if (header_line_markup) set_tag("subj:"); else return TOKEN; }
<INITIAL>^Date:.*|Delivery-Date:.*		;
<INITIAL>^Message-ID:.*				;
<INITIAL>^\tid\ {ID}				;

<INITIAL>E?SMTP\ id\ {ID}			;
<INITIAL>boundary=[ ]*\"?{MIME_BOUNDARY}\"?	{ mime_boundary_set(yy_text()); }
<INITIAL>name=\"?				;
<INITIAL>filename=\"?				;
<INITIAL>^[ \b\t]*\n				{ got_emptyline(); 
						  lineno += 1;
						  if (get_content_type() == MIME_TEXT_HTML)
						      BEGIN(HTML);
						  else
						      BEGIN(TEXT); 
						  return(EMPTY);
						}
^--{MIME_BOUNDARY}(--)?$			{ if (got_mime_boundary(yy_text())) {
						      BEGIN(INITIAL);
						      return BOUNDARY;
						  } else {
						      yyless(2);
						  }
						}

  /* This has to match just as much or more than the below rules, so as to be the 
     controlling rule. */
<HTML>{TOKEN}{HTMLTOKEN}*{BREAKHTML}+{HTMLTOKEN}*.?	|

<HTML>{TOKEN}({HTMLTOKEN})+{WHITESPACE}		{ 
    			char *chr = memchr(yytext, '<', yyleng);	/* find start of html tag */
			size_t len = chr - yytext;
			yyless(len);
			return TOKEN;
			}

<HTML>{TOKEN}({HTMLTOKEN})+/{NOTWHITESPACE} 	{ reorder_html(); }
<HTML>{A2}({HTMLTOKEN})+/{NOTWHITESPACE}	{ reorder_html(); }
<HTML>{A1}({HTMLTOKEN})+/{NOTWHITESPACE} 	{ reorder_html(); }

<HTML>"<!--"					{ BEGIN SCOMMENT; }
<HTML>"<!"					{ BEGIN (strict_check ? HTOKEN : LCOMMENT ); }
<HTML>"<"(a|img|font){WHITESPACE}		{ BEGIN HTOKEN; }
<HTML>"<"					{ BEGIN HDISCARD; }	/* unknown tag */

<HTOKEN>{TOKEN}					{ if (tokenize_html_tags)     return TOKEN; }
<HDISCARD,LCOMMENT>[^>]*>			{ BEGIN HTML; }		/* discard innards of html tags */
<HSCRIPT>{TOKEN}				{ if (tokenize_html_script)   return TOKEN; }

<HTOKEN,HDISCARD>">"				{ BEGIN HTML; }		/* end of tag; return to normal html processing */
<SCOMMENT>{TOKEN}				;			/* discard innards of html comments */ 
<SCOMMENT>"-->"					{ BEGIN HTML; }		/* end of strict comment; return to normal html processing */

{IPADDR}					{ return IPADDR;}
{TOKEN}						{ return TOKEN;}

${DOLLARS}					{ return TOKEN;}
${DOLLARS}\.{CENTS}				{ return TOKEN;}

.						;			/* ignore character */
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

static void reorder_html(void)
{
    char *chr = memchr(yytext, '<', yyleng);	/* find start of html tag */
    size_t len = chr - yytext;
    char *tmp;
    char *yycopy = xmalloc(yyleng + 1); 	/* +1 for NUL byte below */

    memcpy(yycopy, yytext+len, yyleng-len);	/* copy tag to start of buffer */
    memcpy(yycopy+yyleng-len, yytext, len);	/* copy leading text to end of buffer */
    yycopy[yyleng] = '\0';			/* for debugging */

    for(tmp = yycopy+yyleng-1 ; tmp >= yycopy; tmp--) 
	unput(*tmp);
    xfree(yycopy);
}

/*
 * The following sets edit modes for GNU EMACS
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * End:
 */
