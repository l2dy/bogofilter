/* $Id$ */

#ifndef GLOBALS_H
#define GLOBALS_H

#include <float.h> /* has DBL_EPSILON */
#define EPS	DBL_EPSILON	 /* equality cutoff */

#include "system.h" /* has bool */
#include "common.h" /* has PATH_LEN */

#ifdef __LCLINT__
typedef int  bool;
#endif

/* command line options */
extern	bulk_t	bulk_mode;		/* '-B' */
extern	bool	suppress_config_file;	/* '-C' */
extern	bool	nonspam_exits_zero;	/* '-e' */
extern	bool	fisher;			/* '-f' */
extern	FILE	*fpin;			/* '-I' */
extern	bool	logflag;		/* '-l' */
extern	bool	mbox_mode;		/* '-M' */
extern	char	outfname[PATH_LEN];	/* '-O' */
extern	bool	passthrough;		/* '-p' */
extern	bool	quiet;			/* '-q' */
extern	bool	query;			/* '-Q' */
extern	int	Rtable;			/* '-R' */
extern	bool	terse;			/* '-t' */
extern	int	bogotest;		/* '-X', env("BOGOTEST") */
extern	int	verbose;		/* '-v' */
extern	bool	replace_nonascii_characters;	/* '-n' */

/* config file options */
extern	int	max_repeats;
extern	double	min_dev;
extern	double	ham_cutoff;
extern	double	spam_cutoff;
extern	double	thresh_update;

extern	int	abort_on_error;
extern	bool	stats_in_header;

extern	bool	header_line_markup;	/* -Ph,-PH */

extern	uint	db_cachesize;

extern	const char	*update_dir;
extern	const char	*progname;
extern	const char	*progtype;
/*@observer@*/
extern	const char	*stats_prefix;
extern	const char *const version;
extern	const char *const system_config_file;

/* for msgcounts.c */
extern	u_int32_t	 msgs_good;
extern	u_int32_t	 msgs_bad;

/* for passthrough.c */
extern size_t msg_register_size;
extern char msg_register[256];

extern bool maintain;
extern bool onlyprint;

/* from fisher.h */
#define ROBS		0.010	/* Robinson's s */
#define ROBX		0.415	/* Robinson's x */

#define ROBX_W		".ROBX"

extern	double robs;
extern	double robx;

/* for  bogotune */
extern	bool fBogotune;

/* other */

extern FILE  *fpo;

extern char *bogohome;		/* home directory */

enum passmode { PASS_MEM, PASS_SEEK };
extern enum passmode passmode;

extern	bool	msg_count_file;
extern	bool	unsure_stats;		/* true if print stats for unsures */

#endif
