/* $Id$ */

/*****************************************************************************

NAME:
   rstats.h -- support for debug routines for printing robinson data.

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#ifndef RSTATS_H
#define RSTATS_H

void rstats_init(void);
void rstats_cleanup(void);

void rstats_add(const word_t *token,
		wordprop_t *props);

void rstats_fini(size_t robn, 
		 FLOAT P, 
		 FLOAT Q, 
		 double spamicity);

void rstats_print(void);

#endif	/* RSTATS_H */
