/* $Id$ */

/*****************************************************************************

NAME:
   qp.h -- prototypes and definitions for qp.c

******************************************************************************/

#ifndef	HAVE_QP_H
#define	HAVE_QP_H

#include "word.h"

uint	qp_decode(word_t *word);

#if	0	/* unused */
bool	qp_validate(word_t *word);
#endif

#endif	/* HAVE_QP_H */
