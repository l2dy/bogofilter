/* $Id$ */

/*****************************************************************************

NAME:
   qp.h -- prototypes and definitions for qp.c

******************************************************************************/

#ifndef	HAVE_QP_H
#define	HAVE_QP_H

#include "word.h"

unsigned int	qp_decode(word_t *word);
bool	qp_validate(word_t *word);

#endif	/* HAVE_QP_H */
