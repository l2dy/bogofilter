/* $Id$ */

#ifndef GLOBALS_H
#define GLOBALS_H

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
extern double min_dev;
extern double spam_cutoff;
extern double thresh_stats;

extern const char *progname;
/*@observer@*/
extern const char *stats_prefix;
extern char *directory;
extern char outfname[PATH_LEN];
extern const char *const version;
extern const char *const system_config_file;

#endif
