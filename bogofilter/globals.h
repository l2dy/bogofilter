#ifndef GLOBALS_H
#define GLOBALS_H

extern int verbose;
extern int passthrough;
extern int nonspam_exits_zero;
extern int logflag;

extern int thresh_index;
extern double thresh_stats;
extern double thresh_rtable;

extern const char *progname;
extern const char *stats_prefix;
extern char directory[PATH_LEN + 100];

#endif
