/* $Id$ */

/*****************************************************************************

NAME:
   html.c -- code for processing html.

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#include <ctype.h>
#include <stdlib.h>

#include "common.h"

#include "html.h"
#include "lexer.h"

/* Macro Definitions */

#define	COMMENT_START	"<!--"
#define	COMMENT_START_LEN 4		/* strlen(COMMENT_START) */

#define	COMMENT_END	"-->"
#define	COMMENT_END_LEN 3		/* strlen(COMMENT_END) */

/* Function Declarations */

static int kill_html_comment(buff_t *buff, size_t comment_start);

/* http://www.w3.org/MarkUp/html-spec/html-spec_3.html#SEC3.2.5
**
** Comments:
**
** To include comments in an HTML document, use a comment declaration.  A comment
** declaration consists of `<!' followed by zero or more comments followed by `>'. 
** Each comment starts with `--' and includes all text up to and including the 
** next occurrence of `--'. In a comment declaration, white space is allowed after 
** each comment, but not before the first comment. The entire comment declaration 
** is ignored.
*/

/* Function Definitions */

int process_html_comments(buff_t *buff)
{
    byte *tmp;
    byte *buf_start = buff->t.text;
    byte *buf_used = buf_start + buff->t.leng;

    for (tmp = buf_start; tmp < buf_used && (tmp = memchr(tmp, '<', buf_used-tmp)) != NULL; tmp += 1) {
	int count;
	size_t comment_offset = tmp - buf_start;
	count = kill_html_comment(buff, comment_offset);
	buf_used = buf_start + buff->t.leng;
    }

    if (buff->t.leng != (size_t)(buf_used - buf_start)) {
	fprintf(dbgout, "incorrect count:  %d != %d\n", buff->t.leng, (size_t)(buf_used - buf_start));
	exit(2);
    }

    return buf_used - buf_start;
}

static int kill_html_comment(buff_t *buff, size_t comment_offset)
{
    byte *buf_beg = buff->t.text + comment_offset;
    byte *buf_end = buff->t.text + buff->size;
    byte *comment = NULL;
    byte *buf_used;

    int level = 0;
    bool done = false;
    byte *tmp = buf_beg;

    while (!done) {
	byte c;
	size_t need;
	size_t avail;

/*
	if (test & 1) {
	    size_t cspn = strcspn((const char *)tmp, "<>");
	    tmp += cspn;
	}
*/
	c = *tmp;

	need = (c == '<') ? COMMENT_START_LEN : 1;

	buf_used = buf_beg + buff->t.leng - comment_offset;
	avail = buf_used - tmp;

	if (need > avail) {
	    int new;
	    int used = tmp - buf_beg + comment_offset;
	    new = buff_fill(buff, used, need);
	    if (new == 0 || new == EOF)
		break;
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
		comment = tmp;
		level += 1;
		tmp += COMMENT_START_LEN;
	    }
	    break;
	}
	case '>':
	{
	    /* Hack to only check for ">" rather than complete terminator "-->" */
	    bool short_check = true;
	    if (level == 0)
		done = true;
	    if (level > 0 && (short_check || 
			      memcmp(tmp+1-COMMENT_END_LEN, COMMENT_END, COMMENT_END_LEN) == 0))
	    {
		size_t len = short_check ? 1 : COMMENT_END_LEN;
		tmp += len;
		/* eat comment */
		buff_shift(buff, comment, tmp - comment);
		tmp = comment;
		level -= 1;
		/* If not followed by a comment, there is no need to keep reading */
		if (level == 0 && isalnum(*tmp))
		    done = true;
	    }
	    break;
	}
	default:
	    tmp += 1;
	    done = level == 0;
	}

	/* The NULL check for comment is needed when handling lines 
	** longer than the buffer size.  Compare files msg.line.len.08145
	** (max len of 8145) and msg.line.len.11269 (max len of 11269)
	*/

	/* When killing html comments, there's no need to keep it in memory */
	if (comment != NULL && 
	    buf_end - buf_used < COMMENT_END_LEN) 
	{
	    /* Leave enough to recognize the end of comment string. */
	    size_t shift = tmp - comment;
	    buff_shift(buff, comment, shift);
	    tmp = comment;
	    buf_used -= shift;
	}
    }

    return buff->t.leng - comment_offset;
}
