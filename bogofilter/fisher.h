/* $Id$ */
/*  constants and declarations for robinson-fisher method */

#ifndef	HAVE_FISHER_H
#define	HAVE_FISHER_H

#include <robinson.h>

extern	rf_method_t rf_fisher_method;
extern	void	fis_initialize_constants(void);
extern	double	fis_get_spamicity(int robn, double invlogsum, double logsum, double *invproduct, double *product);

#endif	/* HAVE_FISHER_H */
