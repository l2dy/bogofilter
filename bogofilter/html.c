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

/* Function Declarations */

static int kill_html_comment(byte *buf_start, byte *buf_used, byte *buf_end);

/* Function Definitions */

int process_html_comments(byte *buf /** buffer */,
	size_t used /** count of characters in buffer */,
	size_t size /** total buffer size */)
{
    byte *tmp;
    byte *buf_used = buf + used;
    byte *buf_end  = buf + size;

    for (tmp = buf; tmp < buf_used && (tmp = memchr(tmp, '<', buf_used - tmp)) != NULL; tmp += 1) {
	int count = kill_html_comment(tmp, buf_used, buf_end);
	buf_used = tmp + count;
    }
    return buf_used - buf;
}

static int kill_html_comment(byte *buf_start, byte *buf_used, byte *buf_end)
{
    int level = 0;
    byte *tmp = buf_start;

    while (1) {
	byte c = *tmp;
	size_t need = (c == '<') ? COMMENT_START_LEN : 1;
	if (need > (size_t) (buf_used - tmp)) {
	    int new = buff_fill(need, tmp, buf_used - tmp, buf_end - tmp);
	    if (new == 0 || new == EOF)
		break;
	    buf_used += new;
	    continue;
	}
	if (c == '<') {
	    /* ensure buffer has sufficient characters for test */
	    /* check for comment delimiter */
	    if (memcmp(tmp, COMMENT_START, COMMENT_START_LEN) == 0) {
		if (level == 0 && score_html_comments)
		    html_comment_count = min(count_html_comments, html_comment_count + 1);
		level += 1;
		tmp += COMMENT_START_LEN-1;
	    }
	} 
	if (c == '>') {
	    /* Hack to only check for ">" rather than complete terminator "-->" */
	    bool short_check = true;
	    if (short_check || memcmp(tmp+1-COMMENT_END_LEN, COMMENT_END, COMMENT_END_LEN) == 0) {
		/* eat comment */
		size_t cnt = buf_used - tmp;
		if (kill_html_comments) {
		    memmove(buf_start, tmp+1, cnt + 1);
		    buf_used -= tmp + 1 - buf_start;
		}
		tmp = buf_start;
		level -= 1;
	    }
	}
	else {
	    tmp += 1;
	    if (level == 0)
		break;
	}
	/* When killing html comments, there's no need to keep it in memory */
	if (kill_html_comments && buf_end - buf_used < COMMENT_END_LEN) {
	    /* Leave enough to recognize the end of comment string. */
	    byte *buf_temp = buf_used - COMMENT_END_LEN;
	    size_t count = buf_temp - buf_start;
	    memcpy(buf_start, buf_temp, buf_used - buf_temp);
	    tmp -= count;
	    buf_used -= count;
	}
    }

    return buf_used - buf_start;
}
