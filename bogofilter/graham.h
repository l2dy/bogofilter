/* $Id$ */
/*  constants and declarations for graham */

#ifndef	HAVE_GRAHAM_H
#define	HAVE_GRAHAM_H

#include <method.h>

#define UNKNOWN_WORD		0.4f	/* odds that unknown word is spammish */

#define GRAHAM_GOOD_BIAS	2.0	/* don't give good words more weight */

extern	method_t gra_method;
extern	void	gra_initialize_constants(void);
extern	double	gra_bogofilter(wordhash_t *wordhash, FILE *fp); /*@globals errno@*/
extern	double	gra_compute_spamicity(bogostat_t *bogostats, FILE *fp); /*@globals errno@*/
extern	void	gra_print_bogostats(FILE *fp, double spamicity);
extern	void	gra_cleanup(void);

#endif	/* HAVE_GRAHAM_H */
