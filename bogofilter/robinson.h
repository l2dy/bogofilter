/* $Id$ */
/*  constants and declarations for robinson */

#ifndef	HAVE_ROBINSON_H
#define	HAVE_ROBINSON_H

#include <method.h>

#define ROBS			0.001f	/* Robinson's s */
#define ROBX			0.415f	/* Robinson's x */

#define ROBINSON_GOOD_BIAS	1.0	/* don't give good words more weight */

extern	method_t robinson_method;
extern	void	rob_initialize_constants(void);
extern	double	rob_bogofilter(wordhash_t *wordhash, FILE *fp); /*@globals errno@*/
extern	double	rob_compute_spamicity(wordhash_t *wordhash, FILE *fp); /*@globals errno@*/
extern	void	rob_print_bogostats(FILE *fp, double spamicity);
extern	void	rob_cleanup(void);

#endif	/* HAVE_ROBINSON_H */
