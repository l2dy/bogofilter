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
 */

#include <ctype.h>
#include <stdio.h>

#include <config.h>
#include "common.h"

#include "charset.h"
#include "lexer.h"
#include "mime.h"		/* for mime_*() */
#include "textblock.h"
#include "token.h"		/* for got_from() and got_newline() */

#define YY_DECL token_t yylex(void)
#define YY_INPUT(buf,result,max_size) result = yyinput(buf, max_size)

%}

%option align nounput noyywrap noreject 8bit caseless

UINT8		([01]?[0-9]?[0-9]|2([0-4][0-9]|5[0-5]))
IPADDR		{UINT8}\.{UINT8}\.{UINT8}\.{UINT8}
MIME_BOUNDARY	[0-9a-zA-Z\'()+_,-./:=?#]*

ID		[0-9a-zA-Z-]*
MTYPE		[ \t]*[0-9a-zA-Z/-]*
TOKEN		[^[:blank:][:cntrl:][:digit:][:punct:]][^][:blank:]<>;=():&%$#@!+|/\\{}^\"\?\*,[:cntrl:][]+[^[:blank:][:punct:][:cntrl:]]

%%

charset=\"?{ID}\"?				{ got_charset(yytext); yyredo(yytext, '='); }

^MIME-Version:.*				{ mime_version(yytext, yyleng); yyredo(yytext, ':'); }
^Content-Transfer-Encoding:{MTYPE}		{ mime_encoding(yytext, yyleng); yyredo(yytext, ':'); }
^Content-Type:{MTYPE};?				{ mime_type(yytext, yyleng); yyredo(yytext, ':'); }
^Content-Disposition:{MTYPE}			{ mime_disposition(yytext, yyleng); yyredo(yytext, ':'); }
^From\ 						{ return got_from(yytext); }
^Date:.*|Delivery-Date:.*			;
^Message-ID:.*					;
^\tid\ {ID}					;

E?SMTP\ id\ {ID}				;
boundary=[ ]*\"?{MIME_BOUNDARY}\"?		{ mime_boundary_set(yytext, yyleng); }
name=\"?					;
filename=\"?					;
^--{MIME_BOUNDARY}(--)?$			{ got_mime_boundary(yytext, yyleng); return (BOUNDARY); }

{IPADDR}					{ return(IPADDR);}
{TOKEN}						{ return(TOKEN);}
.						;
^\n						{ got_newline(); return(EMPTY);}
\n						;

%%

/*
 * The following sets edit modes for GNU EMACS
 * Local Variables:
 * mode:c
 * End:
 */
