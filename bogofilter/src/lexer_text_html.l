/* $Id$ */

%{
/*
 * NAME
 *   lexer_text_html.l -- bogofilter's lexical analyzer for html text
 *
 *   01/01/2003 - split out of lexer.l
 *
*/

#include <stdlib.h>

#include <config.h>
#include "common.h"

#include "lexer.h"
#include "mime.h"		/* for mime_*() */
#include "token.h"		/* for got_newline() */

#define YY_DECL token_t yylex(void)
#define YY_INPUT(buf,result,max_size) result = yyinput((byte *)buf, max_size)

%}

%option align nounput noyywrap noreject 8bit caseless

MIME_BOUNDARY	[0-9a-zA-Z\'()+_,-./:=?]*

UINT8		([01]?[0-9]?[0-9]|2([0-4][0-9]|5[0-5]))
IPADDR		{UINT8}\.{UINT8}\.{UINT8}\.{UINT8}
TOKEN		[^[:blank:][:cntrl:][:digit:][:punct:]][^][:blank:]<>;=():&%$#@!+|/\\{}^\"\?\*,[:cntrl:][]+[^[:blank:][:punct:][:cntrl:]]

%%

\<\!--						{ html_comment(+1); }
-->						{ html_comment(-1); }
\<						{ html_tag(1); }
\>						{ html_tag(0); }

^From\ 						{ return (msg_header ? FROM : TOKEN); }
^--{MIME_BOUNDARY}(--)?$			{ return (got_mime_boundary((byte *)yytext, yyleng) ? BOUNDARY : TOKEN); }

{IPADDR}					{ return(IPADDR);}
{TOKEN}						{ return(TOKEN); }
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
