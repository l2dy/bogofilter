/* $Id$ */

/*****************************************************************************

NAME:
   fisher.c -- implements Fisher variant on Robinson algorithm.

******************************************************************************/

#include <stdio.h>
/*
 * #include <math.h>
 * #include <string.h>
 * #include <stdlib.h>
 */

#include <config.h>
#include "common.h"

#include <dcdflib.h>

#include "fisher.h"

#define	RF_DEBUG
#undef	RF_DEBUG

#define FISHER_SPAM_CUTOFF	0.952f
#define FISHER_MIN_DEV		0.1f

rf_method_t rf_fisher_method = {
    {
	"fisher",			/* const char		  *name;		*/
	rob_parm_table,	 		/* m_parm_table		  *parm_table		*/
	fis_initialize_constants,	/* m_initialize_constants *initialize_constants	*/
	rob_bogofilter,	 		/* m_compute_spamicity	  *compute_spamicity	*/
	rob_print_bogostats, 		/* m_print_bogostats	  *print_stats		*/
	rob_cleanup, 			/* m_free		  *cleanup		*/
    },
    fis_get_spamicity			/* rf_get_spamicity	  *get_spamicity	*/
};

double prbf(double x, double df)
{
    int which=1;
    double p, q;
    int status;
    double bound;
    cdfchi(&which, &p, &q, &x, &df, &status, &bound);

#ifdef	RF_DEBUG
    if ( !quiet )
	printf( "which: %d, p: %15.8g, q: %15.8g, x: %12.6f, df: %f, status: %d, bound: %f\n",
		which, p, q, x, df, status, bound);
/*
**	which: 1, p: 1.000000, q: 0.000000, x: 699.817846, df: 202.000000, status: 0, bound: 0.000000
**	which: 1, p: 1.000000, q: 0.000000, x: 377.994523, df: 202.000000, status: 0, bound: 202.000000
*/
#endif

    return(status==0 ? q : 1.0);
}

double fis_get_spamicity(size_t robn, double invlogsum, double logsum, double *invproduct, double *product)
{
    double df = 2.0 * robn;
    double _invproduct = prbf(-2.0 * invlogsum, df);
    double _product = prbf(-2.0 * logsum, df);

    double spamicity = (1.0 + _product - _invproduct) / 2.0;

    *product = _product;
    *invproduct = _invproduct;

#ifdef	RF_DEBUG
    if ( !quiet )
	printf( "%d, invproduct: %f, product: %f, spamicity: %f, invlogsum: %f, logsum: %f\n", 
		robn, _invproduct, _product, spamicity, invlogsum, logsum);
#endif

    return spamicity;
}

void fis_initialize_constants(void)
{
    rob_initialize_with_parameters(FISHER_MIN_DEV, FISHER_SPAM_CUTOFF);
}

/* Done */
