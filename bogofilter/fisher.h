/* $Id$ */
/*  constants and declarations for robinson-fisher method */

#ifndef	HAVE_FISHER_H
#define	HAVE_FISHER_H

#include <robinson.h>

extern 	double prbf(double x, double df);		/* needed by rstats.c */

extern	rf_method_t rf_fisher_method;

#endif	/* HAVE_FISHER_H */
