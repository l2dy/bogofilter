/* $Id$ */

/*****************************************************************************

NAME:
   rstats.h -- support for debug routines for printing robinson data.

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#ifndef HAVE_RSTATS_H
#define HAVE_RSTATS_H

void rstats_init(void);

void rstats_add(const char *token,
		double good,
		double bad,
		double prob);

void rstats_fini(size_t robn, 
		 FLOAT P, 
		 FLOAT Q, 
		 double spamicity);

void rstats_print(void);

#endif	/* HAVE_RSTATS_H */
