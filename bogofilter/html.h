/* $Id$ */

/*****************************************************************************

NAME:
   html.h -- prototypes and definitions for html.c

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#ifndef HTML_H
#define HTML_H

#include "common.h"
#include "globals.h"

/* Global variables */

/* - in globals.h
extern	bool	kill_html_comments;
extern	int	count_html_comments;
extern	bool	score_html_comments;
*/

/* Function Prototypes */

extern int process_html_comments(byte *buf, size_t used, size_t size);
extern int kill_html_comment(byte *comment_start, byte *buf_used, byte *buf_end);

#endif
