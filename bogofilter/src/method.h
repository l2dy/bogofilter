/* $Id$ */

/*
** This defines an object oriented API for accessing 
** the methods needed by an object for spamicity computing.
*/

#ifndef	METHOD_H
#define	METHOD_H

#include "bogofilter.h"
#include "configfile.h"
#include "wordhash.h"

typedef struct bogostat_s bogostat_t;

typedef	void	m_initialize(void);
typedef	double	m_compute_spamicity(wordhash_t *wordhash, FILE *fp) /*@globals errno@*/;
typedef	void	m_print_stats(FILE *fp);
typedef	void	m_cleanup(void);
typedef	double	m_spamicity(void);
typedef	rc_t	m_status(void);

/*
** This defines an object oriented API for accessing 
** the methods needed by an object for spamicity computing.
*/

typedef struct method_s {
    const char		*name;
    const parm_desc	*config_parms;
    m_initialize	*initialize;
    m_compute_spamicity	*compute_spamicity;
    m_spamicity		*spamicity;		/* numeric */
    m_status		*status;		/* string - Yes, No, ... */
    m_print_stats	*print_stats;
    m_cleanup		*cleanup;
} method_t;

extern method_t *method;

/*
** Define instance storage ...
*/

typedef struct stats_s {
    double spamicity;
} stats_t;

extern stats_t  *mth_stats;

/* Functions for use by graham.c, robinson.c, and fisher.c */

extern void mth_initialize(void *s, int _max_repeats, double _min_dev, double _spam_cutoff, double _good_weight);
extern double mth_spamicity(void);
extern rc_t mth_status(void);

#endif	/* METHOD_H */
