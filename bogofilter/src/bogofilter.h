/* $Id$ */
/*  constants and declarations for bogofilter */

#ifndef	BOGOFILTER_H
#define	BOGOFILTER_H

#define EVEN_ODDS	0.5		/* used for words we want to ignore */

#define DEVIATION(n)	fabs((n) - EVEN_ODDS)	/* deviation from average */

extern void initialize_constants(void);
extern rc_t bogofilter(void);
extern void print_stats(FILE *fp);

#endif	/* BOGOFILTER_H */
