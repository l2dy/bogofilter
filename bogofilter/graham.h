/* $Id$ */
/*  constants and declarations for graham */

#ifndef	HAVE_GRAHAM_H
#define	HAVE_GRAHAM_H

#include <method.h>

#define UNKNOWN_WORD		0.4f	/* odds that unknown word is spammish */

#define GRAHAM_GOOD_BIAS	2.0	/* don't give good words more weight */

extern	method_t graham_method;

#endif	/* HAVE_GRAHAM_H */
