/* $Id$ */

/*****************************************************************************

NAME:
   prob.c -- calculate token's spamicity

AUTHORS:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#include "globals.h"

#include "prob.h"

double calc_prob(uint good, uint bad)
{
    int n = good + bad;
    double fw;

    if (n == 0)
	fw = robx;
    else {
	double pw = ((bad / msgs_bad) / (bad / msgs_bad + good / msgs_good));
	fw = (robs * robx + n * pw) / (robs + n);
    }

    return fw;
}
