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

#include "buff.h"

/* Global variables */

extern	bool	strict_check;

/* - in globals.h
extern	bool	kill_html_comments;
extern	int	count_html_comments;
extern	bool	score_html_comments;
*/

/* Function Prototypes */

extern int process_html_comments(buff_t *buff);

#endif	/* HTML_H */
