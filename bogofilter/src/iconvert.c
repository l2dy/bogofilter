/* $Id$ */

/*****************************************************************************

NAME:
   iconvert.c -- provide iconv() support for bogofilter's lexer.

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

/**
 ** Note: 01/07/05
 **
 ** "make check" changes:
 **
 **    t.systest - msg.3.txt
 **		0x92	It’s
 **
 **    t.lexer.mbx - spam.mbx
 **	  msg**4
 **		0xAE	Club®
 **	  msg**20
 **		0xA0	MyNetOffers 
 **		0x93	“Your 
 **		0x94	Account”
 **/

#include "common.h"

#include <stdlib.h>
#include <errno.h>

#include <iconv.h>
#include "iconvert.h"

extern	iconv_t cd;

void iconvert(buff_t *src, buff_t *dst)
{
    while (src->read < src->t.leng) {
	char *inbuf;
	size_t inbytesleft;

	char *outbuf;
	size_t outbytesleft;
	size_t count;
	inbuf = src->t.text + src->read;
	inbytesleft = src->t.leng - src->read;

	outbuf = dst->t.text + dst->t.leng;
	outbytesleft = dst->size - dst->read - dst->t.leng;

	/*
	 * The iconv function converts one multibyte character at a time, and for
	 * each character conversion it increments *inbuf and decrements
	 * *inbytesleft by the number of converted input bytes, it increments
	 * *outbuf and decrements *outbytesleft by the number of converted output
	 * bytes, and it updates the conversion state contained in cd. The
	 * conversion can stop for four reasons:
	 */
	count = iconv(cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);

	if (count == (size_t)(-1)) {

	    int err = errno;
	    switch (err) {

	    case EILSEQ:		/* invalid multibyte sequence */
		/*
		 * 1. An invalid multibyte sequence is encountered
		 * in the input. In this case it sets errno to
		 * EILSEQ and returns (size_t)(-1). *inbuf is left
		 * pointing to the beginning of the invalid
		 * multibyte sequence.
		 */
		inbytesleft -= 1;		/* copy 1 byte */
		outbytesleft -= 1;
		*outbuf++ = *inbuf++;
		if (DEBUG_ICONV(1))
		    fprintf(dbgout, "EILSEQ - t: %p, r: %d, l: %d, s: %d\n", src->t.text, src->read, src->t.leng, src->size);
		break;

		/*
		 * 2. The input byte sequence has been entirely
		 * converted, i.e. *inbytesleft has gone down to
		 * 0. In this case iconv returns the number of
		 * non-reversible conversions performed during
		 * this call.
		 */

	    case EINVAL:		/* incomplete multibyte sequence */
		/*
		 * 3. An incomplete multibyte sequence is
		 * encountered in the input, and the input byte
		 * sequence terminates after it. In this case it
		 * sets errno to EINVAL and returns
		 * (size_t)(-1). *inbuf is left pointing to the
		 * beginning of the incomplete multibyte sequence.
		 */
		inbytesleft -= 1;		/* copy 1 byte */
		outbytesleft -= 1;
		*outbuf++ = *inbuf++;
		if (DEBUG_ICONV(1))
		    fprintf(dbgout, "EINVAL - t: %p, r: %d, l: %d, s: %d\n", src->t.text, src->read, src->t.leng, src->size);
		break;

	    case E2BIG:		/* output buffer has no more room */
		/*
		 * 4. The output buffer has no more room for the
		 * next converted character. In this case it sets
		 * errno to E2BIG and returns (size_t)(-1).
		 */
		if (DEBUG_ICONV(1))
		    fprintf(dbgout, "E2BIG - t: %p, r: %d, l: %d, s: %d\n", src->t.text, src->read, src->t.leng, src->size);
		break;

	    default:
		break;
	    }
	    /*
	     * A different case is when inbuf is NULL or *inbuf is
	     * NULL, but outbuf is not NULL and *outbuf is not
	     * NULL. In this case, the iconv function attempts to
	     * set cd's conversion state to the initial state and
	     * store a corresponding shift sequence at *outbuf. At
	     * most *outbytesleft bytes, starting at *outbuf, will
	     * be written. If the output buffer has no more room
	     * for this reset sequence, it sets errno to E2BIG and
	     * returns (size_t)(-1). Otherwise it increments
	     * *outbuf and decrements *outbytesleft by the number
	     * of bytes written.
	     * 
	     * A third case is when inbuf is NULL or inbuf is
	     * NULL, and outbuf is NULL or outbuf is NULL. In this
	     * case, the iconv function sets cd's conversion state
	     * to the initial state.
	     */
	}
	src->read = src->t.leng - inbytesleft;
	dst->t.leng = dst->size - dst->read - outbytesleft;
    }

    if (src->t.leng != src->read)
	fprintf(stderr, "t: %p, r: %d, l: %d, s: %d\n", src->t.text, src->read, src->t.leng, src->size);
}
