/* $Id$ */
/*  constants and declarations for robinson-fisher method */

#ifndef	HAVE_FISHER_H
#define	HAVE_FISHER_H

#include <robinson.h>

extern 	double prbf(double x, double df);		/* needed by rstats.c */

extern	rf_method_t rf_fisher_method;
extern	void	fis_initialize_constants(void);
extern	double	fis_get_spamicity(size_t robn, FLOAT P, FLOAT Q);

#endif	/* HAVE_FISHER_H */
