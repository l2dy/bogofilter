/* $Id$ */
/*  constants and declarations for bogofilter */

#ifndef	BOGOFILTER_H
#define	BOGOFILTER_H

#define EVEN_ODDS	0.5		/* used for words we want to ignore */

#define DEVIATION(n)	fabs((n) - EVEN_ODDS)	/* deviation from average */

typedef enum rc_e {RC_SPAM=0, RC_HAM=1, RC_UNSURE=2, RC_OK, RC_MORE}  rc_t;

extern void initialize_constants(void);
extern rc_t bogofilter(void);
extern void print_stats(FILE *fp);

#endif	/* BOGOFILTER_H */
