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
#include "lexer.h"

/* ignore words longer than this */
#define MAXWORDLEN 30

extern int yyleng;
extern char *yytext;

bool block_on_subnets = false;

static token_t save_class = NONE;
static char save_text[256];

static void got_encoding(const char *encoding);

token_t get_token(void)
{
    token_t class;
    unsigned char *cp, *t;

    /* If saved IPADDR, truncate last octet */
    if ( block_on_subnets && save_class == IPADDR )
    {
	t = strrchr(save_text, '.');
	if (t == NULL)
	    save_class = NONE;
	else
	{
	    *t = '\0';	
	    yylval = save_text;
	    yyleng = strlen(save_text);
	    return save_class;
	}
    }

    /* If saved Content-Transfer-Encoding, do it now */
    if ( save_class == TRANSFER )
    {
	save_class = NONE;
	get_token();
	got_encoding(yytext);
	return TOKEN;
    }

    while ((class = yylex()) > 0) {
	/* when we have a boundary line, eliminate the distinction between
	 * start and end boundary, chopping of the distinct trailing -- of
	 * the end boundary. */

#if	0
	if (class == FROM) {
	    const char *prefix="From ";
	    size_t len = strlen(prefix);
	    save_class = FROM;
	    strlcpy( save_text, yytext+len, sizeof(save_text));
	    break;
	}
#endif

	if (class == BOUNDARY && yyleng >= 4) {
	    if ((t = strstr(yytext+yyleng-2, "--"))) {
		strcpy(t, "");
	    }
	    break;
	}

	if (class == IPADDR && block_on_subnets)
	{
	    const char *prefix="url:";
	    size_t len = strlen(prefix);
	    size_t avl = sizeof(save_text);
	    yylval = save_text;
	    save_class = IPADDR;
	    avl -= strlcpy( yylval, "url:", avl);
	    yyleng = strlcpy( yylval+len, yytext, avl);
	    return (class);
	}

	if (class == CHARSET)	/* charset=name */
	{			/* Get name; call got_charset(); return name. */
	    get_token();
	    got_charset(yytext);
	}

	if (class == TRANSFER)	/* Content-Transfer-Encoding: encoding */
	{			/* Get encoding; call got_encoding(); return encoding. */
				/* See RFC 1521 & RFC 1522 */
	    save_class = TRANSFER;
	    yytext[--yyleng] = '\0';	/* trim trailing colon */
	}

	switch (class)
	{
	case IPADDR:
	    break;
	case FROM:
	    break;
	case TRANSFER:
	    break;
	case BOUNDARY:
	    break;
	}

	/* eat all long words */
	if (yyleng <= MAXTOKENLEN)
	    break;
    }

    /* Need separate loop so lexer can see "From", "Date", etc */
    for (cp = yytext; *cp; cp++)
	*cp = casefold_table[*cp];

    yylval = yytext;
    return(class);
}

void got_encoding(const char *encoding)
{
    if (verbose > 2 )
	fprintf(stderr, "got_encoding: %s\n", encoding);
}
