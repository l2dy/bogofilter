/* $Id$ */
/*  constants and declarations for robinson */

#ifndef	HAVE_ROBINSON_H
#define	HAVE_ROBINSON_H

#include "wordhash.h"

#define EVEN_ODDS	0.5f		/* used for words we want to ignore */
#define UNKNOWN_WORD	0.4f		/* odds that unknown word is spammish */

#define ROBS			0.001f	/* Robinson's s */
#define ROBX			0.415f	/* Robinson's x */

#define ROBINSON_GOOD_BIAS	1.0	/* don't give good words more weight */

extern	void	rob_initialize_constants(void);
extern	double	rob_bogofilter(wordhash_t *wordhash, FILE *fp); /*@globals errno@*/
extern	double	rob_compute_spamicity(wordhash_t *wordhash, FILE *fp); /*@globals errno@*/
extern	void	rob_print_bogostats(FILE *fp, double spamicity);

#endif	/* HAVE_ROBINSON_H */
