/* $Id$ */
/*  constants and declarations for method */

#ifndef	HAVE_METHOD_H
#define	HAVE_METHOD_H

#include <bogoconfig.h>
#include <wordhash.h>

typedef struct bogostat_s bogostat_t;

typedef	void	m_initialize(void);
typedef	double	m_compute_spamicity(wordhash_t *wordhash, FILE *fp); /*@globals errno@*/
typedef	void	m_print_bogostats(FILE *fp, double spamicity);
typedef	void	m_cleanup(void);

/*
** This defines an object oriented API for accessing 
** the methods needed by an object for spamicity computing.
*/

typedef struct method_s {
    const char		*name;
    const parm_desc	*config_parms;
    m_initialize	*initialize;
    m_compute_spamicity	*compute_spamicity;
    m_print_bogostats	*print_stats;
    m_cleanup		*cleanup;
} method_t;

extern method_t *method;

#endif	/* HAVE_METHOD_H */
