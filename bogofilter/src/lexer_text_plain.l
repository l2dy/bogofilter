/* $Id$ */

%{
/*
 * NAME
 *   lexer_text_plain.l -- bogofilter's lexical analyzer for plain text
 *
 *   01/01/2003 - split out of lexer.l
 *
*/

#include <stdlib.h>

#include <config.h>
#include "common.h"

#include "lexer.h"
#include "mime.h"		/* for mime_*() */
#include "token.h"

#define YY_DECL token_t yylex(void)
#define YY_INPUT(buf,result,max_size) result = yyinput((byte *)buf, max_size)

static word_t yy_text;
%}

%option align nounput noyywrap noreject 8bit caseless
%option prefix="text_plain_"

MIME_BOUNDARY	[0-9a-zA-Z\'()+_,-./:=?#]{1,70}

UINT8		([01]?[0-9]?[0-9]|2([0-4][0-9]|5[0-5]))
IPADDR		{UINT8}\.{UINT8}\.{UINT8}\.{UINT8}
TOKEN		[^[:blank:][:cntrl:][:digit:][:punct:]][^][:blank:]<>;=():&%$#@!+|/\\{}^\"\?\*,[:cntrl:][]+[^[:blank:][:punct:][:cntrl:]]

%%

^From\ 						{ yy_text.text=(byte *)yytext; yy_text.leng=yyleng;
						  return (msg_header && is_from(&yy_text) ? FROM : TOKEN); }
^--{MIME_BOUNDARY}(--)?$			{ yy_text.text=(byte *)yytext; yy_text.leng=yyleng;
						  return (got_mime_boundary(&yy_text) ? BOUNDARY : TOKEN); }

{IPADDR}					{ return(IPADDR);}
{TOKEN}						{ return(TOKEN); }
.						;
^\n						{ got_emptyline(); return(EMPTY);}
\n						;

%%

/*
 * The following sets edit modes for GNU EMACS
 * Local Variables:
 * mode:c
 * End:
 */
