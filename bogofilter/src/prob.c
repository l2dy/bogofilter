/* $Id$ */

/*****************************************************************************

NAME:
   prob.c -- calculate token's spamicity

AUTHORS:
   David Relson <relson@osagesoftware.com>
   Matthias Andree <matthias.andree@gmx.de>

******************************************************************************/

#include "globals.h"
#include "prob.h"

double calc_prob_pure(uint good, uint bad,
	uint goodmsgs, uint badmsgs,
	double s, double x)
{
    int n = good + bad;
    double fw, pw;

    /* http://www.linuxjournal.com/article.php?sid=6467 */

    /* robs is Robinson's s parameter, the "strength of background info" */
    /* robx is Robinson's x parameter, the assumed probability that
     * a word we don't have enough info about will be spam */
    /* n is the number of messages that contain the word w */

    if (n == 0)
	fw = robx;
    else {
	/* The original version of this code has four divisions.
	pw = ((bad / badmsgs) / (bad / badmsgs + good / goodmsgs));
	*/

	/* This modified version, with 1 division, is considerably% faster. */
	pw =   bad * (double)goodmsgs
	    / (bad * (double)goodmsgs + good * (double)badmsgs);

	fw = (s * x + n * pw) / (s + n);
    }

    return fw;
}

double calc_prob(uint good, uint bad)
{
    return calc_prob_pure(good, bad, msgs_good, msgs_bad, robs, robx);
}
