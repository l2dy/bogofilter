/* $Id$ */

/*****************************************************************************

NAME:
   rstats.h -- support for debug routines for printing robinson data.

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

void rstats_init(void);

void rstats_add(char *token,
		double good,
		double bad,
		double prob);

void rstats_fini(int robn, 
		 double invlogsum, 
		 double logsum, 
		 double invproduct, 
		 double product, 
		 double spamicity);

void rstats_print(void);
