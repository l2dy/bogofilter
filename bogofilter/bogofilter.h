/* $Id$ */
/*  constants and declarations for bogofilter */

#ifndef	HAVE_BOGOFILTER_H
#define	HAVE_BOGOFILTER_H

#include <wordlists.h>

#define EVEN_ODDS	0.5f		/* used for words we want to ignore */
#define UNKNOWN_WORD	0.4f		/* odds that unknown word is spammish */

#define ROBS			0.001f	/* Robinson's s */
#define ROBX			0.415f	/* Robinson's x */

#ifdef	GRAHAM_AND_ROBINSON
#define GRAHAM_GOOD_BIAS	2.0	/* don't give good words more weight */
#define ROBINSON_GOOD_BIAS	1.0	/* don't give good words more weight */
#define GOOD_BIAS (algorithm == AL_GRAHAM ? GRAHAM_GOOD_BIAS : ROBINSON_GOOD_BIAS)
#else
#ifdef	ENABLE_GRAHAM_METHOD
#define GOOD_BIAS		2.0	/* don't give good words more weight */
#endif
#ifdef	ENABLE_ROBINSON_METHOD
#define GOOD_BIAS		1.0	/* don't give good words more weight */
#endif
#endif

typedef enum rc_e {RC_SPAM=0, RC_NONSPAM=1}  rc_t;

extern void initialize_constants(void);
extern rc_t bogofilter(/*@out@*/ double *xss);
extern void print_bogostats(FILE *fp, double spamicity);

#endif	/* HAVE_BOGOFILTER_H */
