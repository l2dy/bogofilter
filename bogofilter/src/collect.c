/* $Id$ */

/* collect.c -- tokenize input and cap word frequencies, return a wordhash */

#include "common.h"

#include <stdlib.h>

#include "charset.h"
#include "mime.h"
#include "wordhash.h"
#include "token.h"

#include "collect.h"

void mime_type2(word_t * text);

void wordprop_init(void *vwordprop)
{
    wordprop_t *wp = vwordprop;
    memset(wp, 0, sizeof(*wp));
}

void wordcnts_init(void *vwordcnts)
{
    wordcnts_t *wc = vwordcnts;
    memset(wc, 0, sizeof(*wc));
}

void wordcnts_incr(wordcnts_t *w1, wordcnts_t *w2)
{
    w1->good += w2->good;
    w1->bad  += w2->bad;
}

/* Tokenize input text and save words in the wordhash_t hash table.
 *
 * Returns:  true if the EOF token has not been read.
 */
void collect_words(wordhash_t *wh)
{
    if (DEBUG_WORDLIST(2)) fprintf(dbgout, "### collect_words() begins\n");

    lexer_init();

    for (;;){
	wordprop_t *wp;
	word_t *token;
	token_t cls = get_token();

	if (cls == NONE)
	    break;

	token = yylval;

	if (cls == BOGO_LEX_LINE)
	{
	    char *s = (char *)(yylval->text+1);/* skip leading quote mark */
	    char *f = memchr(s, '"', yylval->leng - 1);
	    token->text = (unsigned char *) s;
	    token->leng = f - s;
	}

	wp = wordhash_insert(wh, token, sizeof(wordprop_t), &wordprop_init);
	if (wh->type != WH_CNTS &&
	    wp->freq < max_repeats)
	    wp->freq += 1;

	wh->count += 1;

/******* EK **********/

#ifdef	CP866
/* mime charset hack */
	{
	    static bool hascharset=false;
	    if (hascharset)  /* prev token == charset */
	    {
		if(token->leng > 5) {
		    if(!strncmp(token->text, "mime:", 5)) {
			set_charset(token->text+5);
		    }
		}
	    }
	    hascharset = 0;
	    if (token->leng == 5+7)
	    {
		if (!strncmp(token->text, "mime:", 5) &&
		    !strncasecmp(token->text+5, "charset", 7))
		    hascharset = true;
	    }
	}
#endif

#ifdef	CP866_XXX
/* breaks "make check", specifically t.grftest and t.bulkmode -- DR 01/02/05 */
/* EK binary problem hack */
	if (token->leng > 8)
	{ 
	    char str[80];
	    static int binflag=0;
	    int l;
	    l = token->leng;
	    if (l > 40) l = 40;
	    strncpy(str,token->text,l);
	    str[l] = 0;
	    if (!strncasecmp(str, "Content-Type", 12))
	    {  
		binflag++;
	    } else if (binflag == 1) {
		mime_type2(token);
		binflag++;
	    } else
		binflag = 0;
	}
#endif

/******* end of EK addition **********/

	if (DEBUG_WORDLIST(3)) {
	    fprintf(dbgout, "%3d ", (int) wh->count);
	    word_puts(token, 0, dbgout);
	    fputc('\n', dbgout);
	}

	if (cls == BOGO_LEX_LINE)
	{
	    char *s = (char *)yylval->text;
	    s += yylval->leng + 2;
	    wp->cnts.bad = atoi(s);
	    s = strchr(s+1, ' ') + 1;
	    wp->cnts.good = atoi(s);
	    wp->cnts.msgs_good = msgs_good;
	    wp->cnts.msgs_bad = msgs_bad;
	}
    }
    
    if (DEBUG_WORDLIST(2)) fprintf(dbgout, "### collect_words() ends\n");

    return;
}
