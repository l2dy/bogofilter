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

double calc_prob(uint good, uint bad)
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
	pw = ((bad / msgs_bad) / (bad / msgs_bad + good / msgs_good));
	*/

	/* This modified version, with 1 division, is considerably% faster. */
	pw = bad * msgs_good / (bad * msgs_good + good * msgs_bad);

	fw = (robs * robx + n * pw) / (robs + n);
    }

    return fw;
}
