/* $Id$ */

#ifndef GLOBALS_H
#define GLOBALS_H

extern int nonspam_exits_zero;	/* '-e' */
extern int force;		/* '-f' */
extern int logflag;		/* '-l' */
extern int quiet;		/* '-q' */
extern int passthrough;		/* '-p' */
extern int verbose;		/* '-v' */

extern bool stats_in_header;

extern int thresh_index;
extern double thresh_stats;
extern double thresh_rtable;

extern const char *progname;
extern const char *stats_prefix;
extern char directory[PATH_LEN + 100];

#endif
