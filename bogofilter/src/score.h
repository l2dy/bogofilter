/* $Id$ */
/*  constants and declarations for robinson method */

#ifndef	FISHER_H
#define	FISHER_H

#include "wordhash.h"

#define HAM_CUTOFF	0.00	/* 0.00 for two-state, 0.10 for three-state */
#define SPAM_CUTOFF	0.95
#define MIN_DEV		0.10

#define MAX_REPEATS	1	/* cap on word frequency per message */
#define GOOD_BIAS	1.0	/* don't give good words more weight */

extern	bool	first_match;
extern	bool	degen_enabled;

/*
** Define a struct so stats can be saved for printing.
*/

typedef struct score_s {
    double spamicity;
    u_int32_t robn;
    double p_ln;	/* Robinson P, as a log*/
    double q_ln;	/* Robinson Q, as a log*/
    double p_pr;	/* Robinson P */
    double q_pr;	/* Robinson Q */
} score_t;

extern	void	score_initialize(void);
extern	void	score_cleanup(void);

extern	double	msg_compute_spamicity(wordhash_t *wordhash, FILE *fp) /*@globals errno@*/;
extern	double	msg_spamicity(void);
extern	rc_t	msg_status(void);
extern	void	msg_print_stats(FILE *fp);
extern	void	msg_print_summary(void);

/* needed by degen.c */
extern	double	msg_lookup_and_score(const word_t *token, wordcnts_t *cnts);

#endif	/* FISHER_H */
