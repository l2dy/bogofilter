/* $Id$ */
/*
 * $Log$
 * Revision 1.8  2002/09/25 00:02:44  relson
 * Ignore older X-Spam-[...Whatever...] lines.
 *
 * Revision 1.7  2002/09/23 11:31:53  m-a
 * Unnest comments, and move $ line down by one to prevent CVS from adding nested comments again.
 *
 * Revision 1.6  2002/09/22 21:24:36  relson
 * Modify the lexer to allow the full range of alphabetic characters.
 * Thanks to Clint Adams for the new token matching expression.
 *
 * Revision 1.5  2002/09/18 22:30:22  relson
 * Created lexer.h with the definitions needed by lexer_l.l from bogofilter.h.
 * This removes the compile-time dependency between the two files.
 *
 * Revision 1.4  2002/09/18 20:56:43  m-a
 * Let automake deal with the lexer.
 *
 * Revision 1.3  2002/09/16 18:58:14  m-a
 * Fix 'last line occasionally emitted twice' bug, cleaning up our yyinput().
 *
 * Revision 1.2  2002/09/15 15:52:24  relson
 *
 *
 * Makefile.in:
 * 	- fix .l.c rule so that lexer_l.c is correctly generated from lexer_l.l
 * 	- added lexer_l.c to target mostlyclean-compile
 * 	- removed lexer_l.c from  DIST_COMMON.  As it can can be generated, it no longer needs to be distributed.
 * 	- added target lexertest (from original bogofilter release)
 *
 * lexer_l.l:
 * 	- defined global variable passthrough so that linking lexertest succeeds.
 *
 * Revision 1.1.1.1  2002/09/14 22:15:20  adrian_otto
 * 0.7.3 Base Source
 * */
%{
/*
 * NAME
 *   lexer_l.l -- bogofilter's lexical analyzer
 *
*/
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "lexer.h"

// Our lexical analysis is different from Paul Graham's rules: 
//
// We throw away headers that are readily identifiable as dates.
// We throw away text lines that look like BASE64 encoding.
// We throw away all digit strings that don't look like IP address parts.
// We thow away lines beginning with <tab>id<space> -- mailer UDs.
// We throw away various bits of MIME cruft (things that look like
// declarations and boundaries for multipart declarations).
// We throw away *all* tokens of length 1 or 2.
//
// These are optimizations to keep the token lists from bloating.
// The big win is regognizing machine-generated unique IDs that
// we'll never see again and shouldn't 
//
// We don't treat dot between two alphanumerics as a separator,
// because we want to keep domain names and IP addresses together as 
// recognizable units. 
//
// Having done the above, there isn't much need to recognize URLs.  
// If a URL is a spam indicator, very likely any other URL from the
// same site is as well, so the hostname part should be an adequate
// statistical trigger.  
//
// The list of HTML keywords and attributes to be ignored is from the 4.0 DTD.

extern int passthrough;

struct textblock textblocks, *textend = &textblocks;

#define YY_INPUT(buf,result,max_size) result = yyinput(buf, max_size)

int yyinput(buf, max_size)
// input getter for the scanner
{
    char *returned;

    returned = fgets((char *)buf, max_size, yyin);
    if (returned == NULL) {
	if (ferror(yyin)) {
	    fprintf(stderr, "input in flex scanner failed\n");
	    exit(2);
	} else {
	    return 0;
	}
    }

    if (passthrough)
    {
	// Also, save the text on a linked list of lines.
	// Note that we store fixed-length blocks here, not lines.
	// One very long physical line could break up into more
	// than one of these.
	textend->block = strdup((char *)buf);
	textend->next = (struct textblock *)malloc(sizeof(struct textblock));
	textend->next->block = (char *)NULL;
	textend = textend->next;
    }

    return(strlen(returned));
}

%}

%option full align nounput noyywrap noreject 8bit

BASE64		[A-Za-z0-9/+]
IPADDR		[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+
MIME_BOUNDARY	^--[^[:blank:]\n]*$

%%

abbr	;
above	;
acronym	;
accesskey	;
align	;
all	;
alt	;
alink	;
applet	;
archive	;
axis	;
basefont	;
baseline	;
below	;
bgcolor	;
big	;
body	;
border	;
bottom	;
box	;
button	;
cellpadding	;
cellspacing	;
center	;
char	;
charoff	;
charset	;
circle	;
cite	;
colspan	;
coords	;
class	;
classid	;
clear	;
codebase	;
codetype	;
color	;
cols	;
compact	;
content	;
datetime	;
declare	;
defer	;
data	;
default	;
dfn	;
dir	;
disabled	;
face	;
font	;
frameborder	;
groups	;
head	;
headers	;
height	;
href	;
hreflang	;
hsides	;
http-equiv	;
hspace	;
iframe	;
input	;
img	;
ismap	;
justify	;
kbd	;
label	;
lang	;
language	;
left	;
lhs	;
link	;
longdesc	;
map	;
marginheight	;
marginwidth	;
media	;
meta	;
middle	;
multiple	;
name	;
nohref	;
none	;
noresize	;
noshade	;
nowrap	;
object	;
onblur	;
onchange	;
onclick	;
ondblclick	;
onfocus	;
onmousedown	;
onmouseup	;
onmouseover	;
onmousemove	;
onmouseout	;
onkeypress	;
onkeydown	;
onkeyup		;
onload	;
onselect	;
onunload	;
param	;
poly	;
profile	;
prompt	;
readonly	;
rect	;
rel	;
rev	;
rhs	;
right	;
rows	;
rowspan	;
rules	;
samp	;
scheme	;
scope	;
script	;
scrolling	;
select	;
selected	;
shape	;
size	;
small	;
span	;
src	;
standby	;
strike	;
strong	;
style	;
sub	;
summary	;
sup	;
tabindex	;
table	;
target	;
textarea	;
title	;
top	;
type	;
usemap	;
valign	;
value	;
valuetype	;
var	;
vlink	;
vsides	;
void	;
vspace	;
width	;
\<\!--	;
-->	;

^From\ 					{return(FROM);}
^Date:.*|Delivery-Date:.*			;
^Message-ID:.*					;
^X-Spam-[A-Za-z]+:.*				;
^{BASE64}+$					;
^\tid\ .*					;

SMTP\ id\ .*					;
boundary=.*					;
name=\"						;
filename=\"					;
{MIME_BOUNDARY}					;

{IPADDR}					{return(TOKEN);}
[^[:blank:]\n[:digit:][:punct:]][^][:blank:]<>;=():&%$#@!+|/\\{}^\"?\*,\n[]+[^[:blank:][:punct:]\n]	{return(TOKEN);}
.						;
\n						;

%%

int get_token(void)
{
    int class;
    char *cp;

    while ((class = yylex()) > 0)
	if (yyleng <= MAXWORDLEN)
	    break;

    for (cp = yytext; *cp; cp++)
	*cp = tolower(*cp);
    return(class);
}

#ifdef MAIN
int passthrough;
int main(void)
{
    int	t;

    while ((t = get_token()) > 0)
    {
	(void) printf("get_token: %d '%s'\n", t, yytext);
    }
    return 0;
}
#endif /* MAIN */

// The following sets edit modes for GNU EMACS
// Local Variables:
// mode:fundamental
// End:
