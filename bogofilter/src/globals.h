/* $Id$ */

#ifndef GLOBALS_H
#define GLOBALS_H

#define	VER_017	/* for 0.17.x */
#undef	VER_017	/* for 0.16.x */

#include <float.h> /* has DBL_EPSILON */
#define EPS	DBL_EPSILON	 /* equality cutoff */

#include "system.h" /* has bool */
#include "common.h" /* has PATH_LEN */

#ifdef __LCLINT__
typedef int  bool;
#endif

/* command line options */
#ifdef	ENABLE_DEPRECATED_CODE
extern	bool	twostate;		/* '-2' */
extern	bool	threestate;		/* '-3' */
#endif
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
#ifdef	ENABLE_DEPRECATED_CODE
extern	wl_t	wl_mode;		/* '-W' */
#endif

/* config file options */
extern	int	max_repeats;
extern	double	min_dev;
extern	double	ham_cutoff;
extern	double	spam_cutoff;
#ifdef	ENABLE_DEPRECATED_CODE
extern	double	thresh_stats;
#endif
extern	double	thresh_update;

extern	int	abort_on_error;
extern	bool	stats_in_header;

/* for lexer_v3.l */
#ifdef	ENABLE_DEPRECATED_CODE
extern	bool	header_degen;		/* -H 	   - default: disabled */
extern	bool	first_match;		/* -Pf,-PF - default: disabled */
extern	bool	degen_enabled;		/* -Pd,-PD - default: disabled */
#endif

extern	bool	header_line_markup;	/* -Ph,-PH */

#ifndef	ENABLE_DEPRECATED_CODE
#ifndef	VER_017
#define	ignore_case		false	/* -PI */
#define	tokenize_html_tags 	true	/* -Pt */
#define	tokenize_html_script 	false	/* -Ps - not yet in use */
#endif
#else
extern	bool	ignore_case;		/* -Pu,-PU */
extern	bool	tokenize_html_tags;	/* -Pt,-PT */
extern	bool	tokenize_html_script;
#if	0
extern	bool	separate_counts;	/* -Ps,-PS - default: enabled  */
#endif
#endif

extern	unsigned int	db_cachesize;

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

/* from html.h */
#ifndef	ENABLE_DEPRECATED_CODE
#ifndef	VER_017
#define strict_check	false
#endif
#else
extern	bool strict_check;
#endif

/* from robinson.h */
#define ROBS			0.010	/* Robinson's s */
#define ROBX			0.415	/* Robinson's x */

#define ROBX_W			 ".ROBX"

extern	double robs;
extern	double robx;

/* for  bogotune */
extern	bool fBogotune;

/* other */

enum passmode { PASS_MEM, PASS_SEEK };
extern enum passmode passmode;

extern	bool	msg_count_file;
extern	bool	unsure_stats;		/* true if print stats for unsures */

#endif
