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

/* Function Declarations */

static int kill_html_comment(buff_t *buff, size_t comment_start);

bool strict_check = false;

/* If strict_check is enabled, bogofilter will check for  "<!--" and "-->".
** If strict_check is disabled, bogofilter will check for  "<!" and ">".
**
** The strict mode corresponds to the comment definition at:
**
** 	http://www.w3.org/MarkUp/html-spec/html-spec_3.html#SEC3.2.5
**
**	"To include comments in an HTML document, use a comment declaration.  A 
**	comment declaration consists of `<!' followed by zero or more comments 
**	followed by `>'.   Each comment starts with `--' and includes all text 
**	up to and including the next occurrence of `--'. In a comment declar-
**	ation, white space is allowed after each comment, but not before the 
**	first comment. The entire comment declaration is ignored."
*/

/* Function Definitions */

int process_html_comments(buff_t *buff)
{
    byte *tmp;
    byte *buf_start = buff->t.text;
    byte *buf_used = buf_start + buff->t.leng;

    for (tmp = buf_start; tmp < buf_used &&
	    (tmp = memchr(tmp, (int)(unsigned char)'<', (size_t)(buf_used-tmp))) != NULL;
	    tmp += 1) {
	int count;
	size_t comment_offset = tmp - buf_start;
	count = kill_html_comment(buff, comment_offset);
	buf_used = buf_start + buff->t.leng;
	if (msg_header)		/* If message header encoutered, ... */
	    break;
    }

    if (buff->t.leng != (size_t)(buf_used - buf_start)) {
	fprintf(dbgout, "incorrect count:  %ld != %ld\n",
		(long)buff->t.leng, (long)(buf_used - buf_start));
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

    const char *start = strict_check ? "<!--" : "<!";
    const char *finish = strict_check ? "-->" : ">";
    size_t start_len = strlen(start);
    size_t finish_len = strlen(finish);

    while (!done) {
	byte c;
	size_t need;
	size_t avail;

/*
	fails for msg.spam.sky.3343.2

	if (test & 1) {
	    size_t cspn = strcspn((const char *)tmp, "<>");
	    tmp += cspn;
	}
*/
	c = *tmp;

	need = (c == '<') ? start_len : finish_len;

	buf_used = buf_beg + buff->t.leng - comment_offset;
	avail = buf_used - tmp;

	if (need > avail) {
	    int new;
	    int used = tmp - buf_beg + comment_offset;
	    new = buff_fill(buff, used, need);
	    if (new == 0 || new == EOF)
		break;
	    if (msg_header)	/* If message header encoutered, ... */
		done = true;
	    continue;
	}

	switch (c)
	{
	case '<':
	{
	    /* ensure buffer has sufficient characters for test */
	    /* check for comment delimiter */
	    if (memcmp(tmp, start, start_len) != 0)
		tmp += 1;
	    else {
		comment = tmp;
		level += 1;
		tmp += start_len;
	    }
	    break;
	}
	case '>':
	{
	    if (level == 0)
		done = true;
	    tmp += 1;
	    if (level > 0 && (memcmp(tmp - finish_len, finish, finish_len) == 0))
	    {
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
	    done = (level == 0);
	}

	/* The NULL check for comment is needed when handling lines 
	** longer than the buffer size.  Compare files msg.line.len.08145
	** (max len of 8145) and msg.line.len.11269 (max len of 11269)
	*/

	/* When killing html comments, there's no need to keep it in memory */
	if (comment != NULL && 
	    (size_t)(buf_end - buf_used) < finish_len)
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
