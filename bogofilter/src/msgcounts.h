/* $Id$ */

/*****************************************************************************

NAME:
   msgcounts.h -- routines for setting & computing .MSG_COUNT values

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#ifndef MSGCOUNTS_H
#define MSGCOUNTS_H

extern	long msgs_good;
extern 	long msgs_bad;

void compute_msg_counts(void);
void set_msg_counts(char *s);

#endif	/* MSGCOUNTS_H */
