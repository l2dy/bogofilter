/* $Id$ */
/*  constants and declarations for method */

#ifndef	HAVE_METHOD_H
#define	HAVE_METHOD_H

#include <wordhash.h>

typedef struct bogostat_s bogostat_t;

typedef	void	m_initialize(void);
#if	1
typedef	double	m_compute_spamicity(wordhash_t *wordhash, FILE *fp); /*@globals errno@*/
#else
typedef	double	m_bogofilter(wordhash_t *wordhash, FILE *fp); /*@globals errno@*/
typedef	double	m_compute_spamicity(bogostat_t *bogostats, FILE *fp); /*@globals errno@*/
#endif
typedef	void	m_print_bogostats(FILE *fp, double spamicity);
typedef	void	m_cleanup(void);

typedef struct method_s {
    
    m_initialize		*initialize;
    m_compute_spamicity		*compute_spamicity;
    m_print_bogostats		*print_stats;
    m_cleanup			*cleanup;

} method_t;

extern method_t *method;

#endif	/* HAVE_METHOD_H */
