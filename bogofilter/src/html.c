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

static int kill_html_comment(buff_t *buff);

/* Function Definitions */

int process_html_comments(buff_t *buff)
{
    byte *tmp;
    int count;
    byte *buf = buff->t.text;
    byte *buf_used = buf + buff->t.leng;

    for (tmp = buf; tmp < buf_used && (tmp = memchr(tmp, '<', buf_used-tmp)) != NULL; tmp += 1) {
	buff_shrink(buff, tmp - buf);
	count = kill_html_comment(buff);
	buf_used = tmp + count;
	buff_expand(buff, tmp - buf);
    }
    return buf_used - buf;
}

static int kill_html_comment(buff_t *buff)
{
    byte *buf_start = buff->t.text;
    byte *buf_used  = buf_start + buff->t.leng;
    byte *buf_end   = buf_start + buff->size;

    int level = 0;
    bool done = false;
    byte *tmp = buf_start;

    while (!done) {
	byte c;
	size_t need;
	size_t avail;

	if (test) {
	    size_t cspn = strcspn((const char *)tmp, "<>");
	    tmp += cspn;
	} 
	c = *tmp;

	need = (c == '<') ? COMMENT_START_LEN : 1;
	avail = buf_used - tmp;

	if (need > avail) {
	    int new;
	    int used = tmp - buf_start;
	    buff_shrink(buff, used);
	    new = buff_fill(need, buff);
	    buff_expand(buff, used);
	    if (new == 0 || new == EOF)
		break;
	    buf_used += new;
	    continue;
	}

	switch (c)
	{
	case '<':
	{
	    /* ensure buffer has sufficient characters for test */
	    /* check for comment delimiter */
	    if (memcmp(tmp, COMMENT_START, COMMENT_START_LEN) != 0)
		tmp += 1;
	    else {
		if (level == 0 && score_html_comments)
		    html_comment_count = min(count_html_comments, html_comment_count + 1);
		level += 1;
		tmp += COMMENT_START_LEN;
	    }
	    break;
	}
	case '>':
	{
	    /* Hack to only check for ">" rather than complete terminator "-->" */
	    bool short_check = true;
	    if (short_check || memcmp(tmp+1-COMMENT_END_LEN, COMMENT_END, COMMENT_END_LEN) == 0) {
		/* eat comment */
		size_t cnt = buf_used - tmp;
		if (kill_html_comments) {
		    memmove(buf_start, tmp+1, cnt + 1);
		    buf_used -= tmp + 1 - buf_start;
		    buff->read = 0;
		    buff->t.leng = buf_used - buf_start;
		}
		tmp = buf_start;
		if (level > 0)
		    level -= 1;
	    }
	    break;
	}
	default:
	    tmp += 1;
	    done = level == 0;
	}

	/* When killing html comments, there's no need to keep it in memory */
	if (kill_html_comments && buf_end - buf_used < COMMENT_END_LEN) {
	    /* Leave enough to recognize the end of comment string. */
	    byte *buf_temp = buf_used - COMMENT_END_LEN;
	    size_t count = buf_temp - buf_start;
	    memcpy(buf_start, buf_temp, buf_used - buf_temp);
	    tmp -= count;
	    buff->read = 0;
	    buf_used -= count;
	}
    }

    return buf_used - buf_start;
}
