/* $Id$ */

#ifndef GLOBALS_H
#define GLOBALS_H

#include <float.h> /* has DBL_EPSILON */
#define EPS		(100.0 * DBL_EPSILON) /* equality cutoff */

#include "system.h" /* has bool */
#include "common.h" /* has PATH_LEN */

#ifdef __LCLINT__
#define bool int
#endif

/* command line options */
extern	bool	twostate;		/* '-2' */
extern	bool	threestate;		/* '-3' */
extern	bool	suppress_config_file;	/* '-C' */
extern	bool	nonspam_exits_zero;	/* '-e' */
extern	bool	fisher;			/* '-f' */
extern	bool	force;			/* '-F' */
extern	bool	logflag;		/* '-l' */
extern	bool	terse;			/* '-t' */
extern	bool	quiet;			/* '-q' */
extern	bool	passthrough;		/* '-p' */
extern	bulk_t	bulk_mode;		/* '-B' */
extern	int	verbose;		/* '-v' */
extern	FILE	*fpin;			/* '-I' */

extern	int	Rtable;			/* '-R' */
extern	int	test;			/* '-T' */

extern	char	*directory;		/* '-d' */
extern	char	outfname[PATH_LEN];	/* '-O' */

/* config file options */
extern	int	max_repeats;
extern	double	min_dev;
extern	double	ham_cutoff;
extern	double	spam_cutoff;
extern	double	thresh_stats;

extern	int	abort_on_error;
extern	bool	stats_in_header;
extern	bool	tag_header_lines;

extern	int	db_cachesize;

extern	const char	*update_dir;
extern	const char	*progname;
/*@observer@*/
extern	const char	*stats_prefix;
extern	const char *const version;
extern	const char *const system_config_file;

/* other */

enum passmode { PASS_MEM, PASS_SEEK };
extern enum passmode passmode;

#endif
