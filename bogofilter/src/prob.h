/* $Id$ */

/*****************************************************************************

NAME:
   prob.h -- constants and declarations for prob.c

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#ifndef	PROB_H
#define	PROB_H

/** \b DEPRECATED (uses global variables) */
extern double calc_prob(uint good, uint bad);

/** calculate the probability that a token is bad */
extern double calc_prob_pure(uint good, uint bad,
	double goodmsgs, double badmsgs,
	double s, double x);

#endif	/* PROB_H */

