/* $Id$ */

/*****************************************************************************

NAME:
   prob.c -- calculate token's spamicity

AUTHORS:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#include "globals.h"

#include "prob.h"


/* http://www.linuxjournal.com/article.php?sid=6467 */
double calc_prob(uint good, uint bad)
{
    int n = good + bad;
    double fw;

    if (n == 0)
	fw = robx;
    else {
	/* the original code had four divisions rather than one:
	double pw = ((bad / msgs_bad) / (bad / msgs_bad + good / msgs_good));
	*/
	double pw = bad*msgs_good / (bad*msgs_good + good*msgs_bad);

	/* robs is Robinson's s parameter, the "strenght of background info" */
	/* robx is Robinson's x parameter, the assumed probability that
	 * a word we don't have enough info about will be spam */
	/* n is the amount of mails that contain the word w */
	fw = (robs * robx + n * pw) / (robs + n);
    }

    return fw;
}
