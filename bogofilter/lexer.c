/* $Id$ */

/*
 * NAME
 *   lexer.c -- bogofilter's lexical analyzer (control routines)
 *
 *   01/01/2003 - split out of lexer.l
*/

#include <stdlib.h>
#include <ctype.h>

#include <config.h>
#include "common.h"

#include "charset.h"
#include "error.h"
#include "lexer.h"
#include "mime.h"
#include "fgetsl.h"
#include "textblock.h"
#include "xmalloc.h"
#include "xstrdup.h"

char *yylval;

int yylineno;
int msg_header = 1;

static char *yysave = NULL;

extern char *spam_header_name;

#define YY_NULL 0

int yygetline(char *buf, int max_size)
/* input getter for the scanner */
{
    int count;

    static size_t hdrlen = 0;
    if (hdrlen==0)
	hdrlen=strlen(spam_header_name);

    if (yysave == NULL) {
	count = fgetsl(buf, max_size, fpin);

	/* Also, save the text on a linked list of lines.
	 * Note that we store fixed-length blocks here, not lines.
	 * One very long physical line could break up into more
	 * than one of these. */
	if (passthrough)
	    textblock_add(textblocks, buf, count);
	yylineno += 1;
	if (DEBUG_LEXER(0)) fprintf(dbgout, "### %2d %d %s", yylineno, msg_header, buf);
    }
    else {
	count = strlcpy(buf, yysave, max_size-2);
	buf[count++] = '\n';
	buf[count] = '\0';
	xfree(yysave);
	yysave = NULL;
    }

    /* skip spam_header ("X-Bogosity:") lines */
    while (msg_header
	   && count != -1
	   && memcmp(buf,spam_header_name,hdrlen) == 0)
    {
	do {
	    count = fgetsl(buf, max_size, fpin);
	    if (passthrough)
		textblock_add(textblocks, buf, count);
	    yylineno += 1;
	    if (DEBUG_LEXER(0)) fprintf(dbgout, "*** %2d %d %s\n", yylineno, msg_header, buf);

	    if (count != -1 && *buf == '\n')
		break;
	} while (count != -1 && isspace((unsigned char)*buf));
    }

/* unfolding:
** 	causes "^\tid" to be treated as continuation of previous line
** 	hence doesn't match lexer pattern which specifies beginning of line
*/
    while (0 && msg_header) {
	int c = fgetc(fpin);
	if (c == EOF)
	    break;
	if (c == ' ' || c == '\t') {
	    int add;
	    /* continuation line */
	    ungetc(c,fpin);
	    if (buf[count - 1] == '\n') count --;
	    add = fgetsl(buf + count, max_size - count, fpin);
	    if (add == EOF) break;
	    if (passthrough)
		textblock_add(textblocks, buf+count, add);
	    yylineno += 1;
	    if (DEBUG_LEXER(1)) fprintf(dbgout, "*** %2d %d %s\n", yylineno, msg_header, buf+count);
	    count += add;
	} else {
	    ungetc(c,fpin);
	    break;
	}
    }

    if (count == -1) {
	if (ferror(fpin)) {
	    print_error(__FILE__, __LINE__, "input in flex scanner failed\n");
	    exit(2);
	} else {
	    return YY_NULL;
	}
    }

    if (0) { /* debug */
	fprintf(stderr, "%d: ", count);
	fwrite(buf, 1, count, stderr);
	fprintf(stderr, "\n");
    }

    /* \r\n -> \n */
    if (count >= 2 && 0 == strcmp(buf + count - 2, "\r\n")) {
	count --;
	*(buf + count - 1) = '\n';
    }

    return count;
}

int yyinput(char *buf, int max_size)
/* input getter for the scanner */
{
    int i, count, decoded_count;

    count = yygetline(buf, max_size);

    if ((count > 0) && !msg_header){

	decoded_count = mime_decode(buf, count);

	/*change buffer size only if the decoding worked */
	if (decoded_count != 0)
	    count = decoded_count;
    }

    for (i = 0; i < count; i++ )
    {	
	unsigned char ch = buf[i];
	buf[i] = charset_table[ch];
    }
    
    return (count == -1 ? 0 : count);
}

int yyredo(const char *text, char del)
{
    char *tmp;

    if (DEBUG_LEXER(1)) fprintf(dbgout, "yyredo:  %p %s\n", yysave, text );

    /* if already processing saved text, concatenate new after old */
    if (yysave == NULL) {
	tmp = strdup(text);
    }
    else {
	size_t t = strlen(text);
	size_t s = strlen(yysave);
	tmp = xmalloc(s + t + 1);
	memcpy(tmp, yysave, s+1);
	memcpy(tmp+s, text, t+1);
    }
	
    xfree(yysave);
    yysave = tmp;
    if ((tmp = strchr(tmp,del)) != NULL)
	    *tmp++ = ' ';

    return 1;
}

/*
 * The following sets edit modes for GNU EMACS
 * Local Variables:
 * mode:c
 * End:
 */
