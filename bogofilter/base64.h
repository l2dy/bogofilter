/* $Id$ */

/*****************************************************************************

NAME:
   base64.h -- prototypes and definitions for base64.c

******************************************************************************/

#ifndef	HAVE_BASE64_H
#define	HAVE_BASE64_H

#include "common.h"

int base64_decode(byte *buff, size_t size);

#endif	/* HAVE_BASE64_H */
