/* $Id$ */

#ifndef GLOBALS_H
#define GLOBALS_H

#undef	HAVE_CHARSET

#include <float.h> /* has DBL_EPSILON */
#define EPS		(100.0 * DBL_EPSILON) /* equality cutoff */

#include "system.h" /* has bool */

extern int nonspam_exits_zero;	/* '-e' */
extern bool fisher;		/* '-f' */
extern bool force;		/* '-F' */
extern bool logflag;		/* '-l' */
extern bool terse;		/* '-t' */
extern bool quiet;		/* '-q' */
extern int passthrough;		/* '-p' */
extern int verbose;		/* '-v' */

extern bool stats_in_header;

extern int max_repeats;
extern double spam_cutoff;
extern double thresh_stats;

extern const char *progname;
/*@observer@*/
extern const char *stats_prefix;
extern char directory[PATH_LEN + 100];

#endif
