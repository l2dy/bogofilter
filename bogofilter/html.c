/* $Id$ */

/*****************************************************************************

NAME:
   html.c -- code for processing html.

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#include "common.h"
#include "html.h"
#include "lexer.h"

/* Macro Definitions */

#define	COMMENT_START	"<!--"
#define	COMMENT_START_LEN 4		/* strlen(COMMENT_START) */

#define	COMMENT_END	"-->"
#define	COMMENT_END_LEN 3		/* strlen(COMMENT_END) */

/* Global Variables */

int	html_comment_count = 0;

bool	kill_html_comments  = true;	/* config file option:  "kill_html_comments"  */
int	count_html_comments = 5;	/* config file option:  "count_html_comments" */
bool	score_html_comments = false;	/* config file option:  "score_html_comments" */

/* Function Definitions */

int process_html_comments(byte *buf, size_t used, size_t size)
{
    byte *tmp;
    byte *buf_used = buf + used;
    byte *buf_end  = buf + size;

    for (tmp = buf; (tmp = index(tmp, '<')) != NULL; tmp += 1) {
	byte *comment_start = tmp;
	/* ensure buffer has sufficient characters for test */
	if (COMMENT_START_LEN > (size_t) (buf_used - tmp)) {
	    int new = buff_fill(COMMENT_START_LEN, tmp, buf_used - tmp, buf_end - tmp);
	    if (new == EOF)
		break;
	    buf_used += new;
	}
	/* check for comment delimiter */
	if (memcmp(tmp, COMMENT_START, COMMENT_START_LEN) == 0 ) {
	    if (score_html_comments) {
		html_comment_count = min(count_html_comments, html_comment_count + 1 );
	    }
	    if (kill_html_comments) {
		int count = kill_html_comment(comment_start, buf_used, buf_end);
		buf_used = comment_start + count;
	    }
	}
    }
    return buf_used - buf;
}

int kill_html_comment(byte *comment_start, byte *buf_used, byte *buf_end)
{
    byte c;
    int level = 1;
    byte *tmp = comment_start;
    for (c = *tmp; c != '\0'; c = *++tmp) {
	if (c == '<') {
	    /* ensure buffer has sufficient characters for test */
	    if (COMMENT_START_LEN > (size_t) (buf_used - tmp)) {
		int new = buff_fill(COMMENT_START_LEN, tmp, buf_used - tmp, buf_end - tmp);
		if (new == EOF)
		    break;
		buf_used += new;
	    }
	    /* check for comment delimiter */
	    if (memcmp(tmp, COMMENT_START, COMMENT_START_LEN) == 0) {
		level += 1;
		continue;
	    }
	} 
	if (c == '>' && memcmp(tmp+1-COMMENT_END_LEN, COMMENT_END, COMMENT_END_LEN) == 0) {
	    /* eat comment */
	    size_t cnt = buf_used - tmp;
	    memcpy(comment_start, tmp+1, cnt + 1);
	    buf_used -= tmp + 1 - comment_start;
	    tmp = comment_start;
	    level -= 1;
	    if (level == 0)
		break;
	}
    }
    return buf_used - comment_start;
}
