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
extern	bool	twostate;		/* '-2' */
extern	bool	threestate;		/* '-3' */
extern	bulk_t	bulk_mode;		/* '-B' */
extern	bool	suppress_config_file;	/* '-C' */
extern	bool	nonspam_exits_zero;	/* '-e' */
extern	bool	fisher;			/* '-f' */
extern	bool	force;			/* '-F' */
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
extern	wl_t	wl_mode;		/* '-W' */

/* config file options */
extern	int	max_repeats;
extern	double	min_dev;
extern	double	ham_cutoff;
extern	double	spam_cutoff;
extern	double	thresh_stats;

extern	int	abort_on_error;
extern	bool	stats_in_header;

/* for lexer_v3.l */
extern	bool	header_degen;		/* -H 	   - default: disabled */
extern	bool	ignore_case;		/* -Pu,-PU */
extern	bool	header_line_markup;	/* -Ph,-PH */
extern	bool	tokenize_html_tags;	/* -Pt,-PT */
extern	bool	tokenize_html_script;
extern	bool	first_match;		/* -Pf,-PF - default: disabled */
extern	bool	degen_enabled;		/* -Pd,-PD - default: disabled */
#if	0
extern	bool	separate_counts;	/* -Ps,-PS - default: enabled  */
#endif

extern	int	db_cachesize;

extern	const char	*update_dir;
extern	const char	*progname;
extern	const char	*progtype;
/*@observer@*/
extern	const char	*stats_prefix;
extern	const char *const version;
extern	const char *const system_config_file;

/* for msgcounts.c */
extern	long	 msgs_good;
extern 	long	 msgs_bad;

/* from html.h */
extern	bool strict_check;

/* other */

enum passmode { PASS_MEM, PASS_SEEK };
extern enum passmode passmode;

extern	bool	msg_count_file;
extern	bool	no_stats;		/* true if suppress rstats */

#endif
