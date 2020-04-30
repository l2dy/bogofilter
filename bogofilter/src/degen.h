/*****************************************************************************

NAME:
   degen.h -- prototypes and definitions for degen.c

******************************************************************************/

#ifndef	HAVE_DEGEN_H
#define	HAVE_DEGEN_H

#include "word.h"

extern	bool	first_match;
extern	bool	degen_enabled;

double degen(const word_t *token, wordcnts_t *cnts);

#endif	/* HAVE_DEGEN_H */
