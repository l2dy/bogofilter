/* $Id$ */
/*  constants and declarations for robinson method */

#ifndef	HAVE_ROBINSON_H
#define	HAVE_ROBINSON_H

#include <method.h>

#define ROBS			0.001f	/* Robinson's s */
#define ROBX			0.415f	/* Robinson's x */

#define ROBINSON_GOOD_BIAS	1.0	/* don't give good words more weight */

typedef	double	rf_get_spamicity(size_t robn, FLOAT P, FLOAT Q);
typedef	void	rf_print_summary(size_t robn, FLOAT P, FLOAT Q);

/*
** This defines an object oriented API for creating a
** robinson/fisher subclass of method_t.
*/

typedef struct rf_method_s {
    method_t		m;
    rf_get_spamicity	*get_spamicity;
    rf_print_summary	*print_summary;
} rf_method_t;

extern	double	rob_bogofilter(wordhash_t *wordhash, FILE *fp); /*@globals errno@*/
extern	double	rob_compute_spamicity(wordhash_t *wordhash, FILE *fp); /*@globals errno@*/
extern	void	rob_print_bogostats(FILE *fp, double spamicity);
extern	void	rob_cleanup(void);

#ifdef	ENABLE_ROBINSON_METHOD
extern	rf_method_t rf_robinson_method;

/* needed by fisher.c */
extern	const	parm_desc rob_parm_table[];
extern	void	rob_initialize_with_parameters(double _min_dev, double _spam_cutoff);
#endif

#endif	/* HAVE_ROBINSON_H */
