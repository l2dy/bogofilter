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
    uint g = min(good, msgs_good);
    uint b = min(bad,  msgs_bad);
    int n = g + b;
    double fw;

    if (n == 0)
	fw = robx;
    else {
	double bad_cnt  = (double) max(1, msgs_bad);
	double good_cnt = (double) max(1, msgs_good);
	double pw = ((b / bad_cnt) / (b / bad_cnt + g / good_cnt));
	fw = (robs * robx + n * pw) / (robs + n);
    }

    return fw;
}
