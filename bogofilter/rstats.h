/* $Id$ */

/*****************************************************************************

NAME:
   rstats.h -- support for debug routines for printing robinson data.

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

void rtable_init(void);

void rtable_add(char *token,
		double good,
		double bad,
		double prob);

void rtable_fini(int robn, 
		 double invlogsum, 
		 double logsum, 
		 double invproduct, 
		 double product, 
		 double spamicity);

void rtable_print(void);
