/* $Id$ */

%{
/*
 * NAME
 *   lexer.l -- bogofilter's lexical analyzer
 *
 * ORIG:	[A-Za-z$][A-Za-z0-9$'.-]+[A-Za-z0-9$]		{return(TOKEN);}
 * ADAMS:	[^[:blank:]\n[:digit:][:punct:]][^][:blank:]<>;=():&%$#@!+|/\\{}^\"?\*,\n[]+[^[:blank:][:punct:]\n]	{return(TOKEN);}
*/
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "config.h"
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
// The big win is recognizing machine-generated unique IDs that
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

// ignore words longer than this
#define MAXWORDLEN 20

extern int passthrough;

struct textblock textblocks, *textend = &textblocks;

#define YY_INPUT(buf,result,max_size) result = yyinput(buf, max_size)

char *yylval;
static char alt_text[MAXTOKENLEN];

/* returns -1 for EOF or max_size < 2, characters read otherwise */
int myfgets(char *buf, int max_size, FILE *s)
{
	int c;
	char *cp = buf;

	if ((max_size < 2) || feof(s)) return(-1);

	while ((--max_size > 0) && ((c = getc(s)) != EOF)) {
		*cp++ = c;
		if (c == '\n')
			break;
	}

	*cp = '\0'; /* DO NOT ADD ++ HERE! */
	return(cp - buf);
}


int yyinput(char *buf, int max_size)
// input getter for the scanner
{
    int returned;

    static int hdrlen = 0;
    if (hdrlen==0)
	hdrlen=strlen(SPAM_HEADER_NAME);

    returned = myfgets(buf, max_size, yyin);

    while (returned != -1 && memcmp(buf,SPAM_HEADER_NAME,hdrlen) == 0)
    {
	do {
	    returned = myfgets(buf, max_size, yyin);
	    if (returned != -1 && *buf == '\n')
		break;
	} while (returned != -1 && isspace(*buf));
    }

    if (returned == -1) {
	if (ferror(yyin)) {
	    fprintf(stderr, "input in flex scanner failed\n");
	    exit(2);
	} else {
	    return YY_NULL;
	}
    }

    if (passthrough)
    {
	// Also, save the text on a linked list of lines.
	// Note that we store fixed-length blocks here, not lines.
	// One very long physical line could break up into more
	// than one of these.
	textend->block = (char *)xmalloc(returned);
	textend->len = returned;
	memcpy(textend->block, buf, returned);
	textend->next = (struct textblock *)xmalloc(sizeof(struct textblock));
	textend->next->block = (char *)NULL;
	textend->next->len = 0;
	textend = textend->next;
    }

    /* \r\n -> \n */
    if (returned >= 2 && 0 == strcmp(buf + returned - 2, "\r\n")) {
	returned --;
	*(buf + returned - 1) = '\n';
    }

    return(returned == -1 ? YY_NULL : returned);
}

%}

%option align nounput noyywrap noreject 8bit

BASE64		^([A-Za-z0-9/+]+={1,2}|[A-Za-z0-9/+]{32,})$
UINT8		([01]?[0-9]?[0-9]|2([0-4][0-9]|5[0-5]))
IPADDR		{UINT8}\.{UINT8}\.{UINT8}\.{UINT8}
MIME_BOUNDARY	^--[^[:blank:][:cntrl:]]+

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

^From\ 						{return(FROM);}
^Date:.*|Delivery-Date:.*			;
^Message-ID:.*					;
{BASE64}					;
^\tid\ .*					;

E?SMTP\ id\ .*					;
boundary=.*					;
name=\"?					;
filename=\"?					;
{MIME_BOUNDARY}(--)?$			{return (BOUNDARY);}

{IPADDR}					{return(TOKEN);}
[^[:blank:][:cntrl:][:digit:][:punct:]][^][:blank:]<>;=():&%$#@!+|/\\{}^\"?\*,[:cntrl:][]+[^[:blank:][:punct:][:cntrl:]]	{return(TOKEN);} 
.						;
\n						;

%%

int get_token(void)
{
    int class;
    char *cp, *t;

    alt_text[0] = 0;

    while ((class = yylex()) > 0)
	if (yyleng <= MAXWORDLEN)
	    break;

    /* when we have a boundary line, eliminate the distinction between
     * start and end boundary, chopping of the distinct trailing -- of
     * the end boundary. */
    if (class == BOUNDARY && yyleng >= 4
	    && (t = strstr(yytext+yyleng-2, "--")))
    {
	strcpy(t, "");
    }

    for (cp = yytext; *cp; cp++)
	*cp = tolower(*cp);

    yylval = (alt_text[0] ? alt_text : yytext);
    return(class);
}

// The following sets edit modes for GNU EMACS
// Local Variables:
// mode:fundamental
// End:
