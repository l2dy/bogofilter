/* $Id$ */
/*  constants and declarations for robinson-fisher method */

#ifndef	FISHER_H
#define	FISHER_H

#include "robinson.h"

#define FISHER_HAM_CUTOFF	0.00	/* 0.00 for two-state, 0.10 for three-state */
#define FISHER_SPAM_CUTOFF	0.95
#define FISHER_MIN_DEV		0.10

extern	rf_method_t rf_fisher_method;

#endif	/* FISHER_H */
