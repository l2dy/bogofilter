/* $Id$ */

/*****************************************************************************

NAME:
   fisher.c -- implements Fisher variant on Robinson algorithm.

******************************************************************************/

#include <math.h>
#include <stdio.h>

#include <config.h>
#include "common.h"

#include <dcdflib.h>

#include "fisher.h"

#define	RF_DEBUG
#undef	RF_DEBUG

#define FISHER_SPAM_CUTOFF	0.952f
#define FISHER_MIN_DEV		0.1f

/* Function Prototypes */

static void	fis_initialize_constants(void);
static double	fis_get_spamicity(size_t robn, FLOAT P, FLOAT Q);
static void	fis_print_summary(void);

static double	fis_spamicity(void);
static rc_t	fis_status(void);

/* Static Variables */

rf_method_t rf_fisher_method = {	/* used by config.c */
    {
	"fisher",			/* const char		  *name;		*/
	rob_parm_table,	 		/* m_parm_table		  *parm_table		*/
	fis_initialize_constants,	/* m_initialize_constants *initialize_constants	*/
	rob_bogofilter,	 		/* m_compute_spamicity	  *compute_spamicity	*/
	fis_spamicity,			/* m_spamicity		  *spamicity		*/
	fis_status,			/* m_status		  *status		*/
	rob_print_bogostats, 		/* m_print_bogostats	  *print_stats		*/
	rob_cleanup, 			/* m_free		  *cleanup		*/
    },
    fis_get_spamicity,			/* rf_get_spamicity	  *get_spamicity	*/
    fis_print_summary			/* rf_print_summary	  *print_summary	*/
};

static stats_t stats;

/* Function Definitions */

double prbf(double x, double df)
{
    int which=1;
    double p, q;
    int status;
    double bound;
    cdfchi(&which, &p, &q, &x, &df, &status, &bound);

    return(status==0 ? q : 1.0);
}

double fis_get_spamicity(size_t robn, FLOAT P, FLOAT Q )
{
    double df = 2.0 * robn;
    double ln10 = 2.302585093;		 	/* log(10) - 2.3025850929940459  */

    stats.robn = robn;
    stats.p_ln = log(P.mant) + P.exp * ln10;	/* convert to natural logs */
    stats.q_ln = log(Q.mant) + Q.exp * ln10;	/* convert to natural logs */
    stats.p_pr = prbf(-2.0 * stats.p_ln, df);	/* compute P */
    stats.q_pr = prbf(-2.0 * stats.q_ln, df);	/* compute Q */

    stats.spamicity = (1.0 + stats.q_pr - stats.p_pr) / 2.0;

    return stats.spamicity;
}

void fis_print_summary(void)
{
    (void)fprintf(stdout, "%3d  %-20s  %8.5f  %8.5f  %8.6f  %8.3f  %8.3f\n",
		  stats.robn+1, "P_Q_S_invsum_logsum", 
		  stats.p_pr, stats.q_pr, stats.spamicity, stats.p_ln, stats.q_ln);
}

void fis_initialize_constants(void)
{
    rob_initialize_with_parameters(FISHER_MIN_DEV, FISHER_SPAM_CUTOFF);
}

double fis_spamicity(void)
{
    return stats.spamicity;
}

rc_t fis_status(void)
{
    return ( stats.spamicity >= spam_cutoff ) ? RC_SPAM : RC_NONSPAM ;
}

/* Done */
