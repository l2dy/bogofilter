/* $Id$ */

/*****************************************************************************

NAME:
   base64.h -- prototypes and definitions for base64.c

******************************************************************************/

#ifndef	HAVE_BASE64_H
#define	HAVE_BASE64_H

#include "word.h"

uint	base64_decode(word_t *word);
bool	base64_validate(const word_t *word);

#endif	/* HAVE_BASE64_H */
