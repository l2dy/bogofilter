/* $Id$ */

/*****************************************************************************

NAME:
   globals.c -- define global variables

AUTHOR:
   Matthias Andree <matthias.andree@gmx.de>

******************************************************************************/

#include "system.h"
#include "globals.h"
#include "method.h"

/* exports */

/* command line options */

#ifdef	ENABLE_DEPRECATED_CODE
bool	twostate;			/* '-2' */
bool	threestate;			/* '-3' */
#endif
bulk_t	bulk_mode = B_NORMAL;		/* '-b, -B' */
bool	suppress_config_file;		/* '-C' */
bool	nonspam_exits_zero;		/* '-e' */
FILE	*fpin = NULL;			/* '-I' */
bool	fisher;				/* '-f' */
bool	logflag;			/* '-l' */
bool	mbox_mode;			/* '-M' */
bool	replace_nonascii_characters;	/* '-n' */
bool	passthrough;			/* '-p' */
bool	quiet;				/* '-q' */
bool	query;				/* '-Q' */
int	Rtable = 0;			/* '-R' */
bool	terse;				/* '-t' */
int	bogotest = 0;			/* '-X', env("BOGOTEST") */
int	verbose;			/* '-v' */

/* config file options */
int	max_repeats;
double	min_dev;
double	spam_cutoff;
#ifdef	ENABLE_DEPRECATED_CODE
double	thresh_stats;
#endif
double	thresh_update;

const char	*update_dir;
/*@observer@*/
const char	*stats_prefix;

/* for lexer_v3.l */
#ifdef	ENABLE_DEPRECATED_CODE
bool	header_degen = false;		/* -H 	   - default: disabled */
bool	degen_enabled = false;		/* -Pd,-PD - default: disabled */
bool	first_match = true;		/* -Pf,-PF - default: enabled  */
#endif

#ifndef	ENABLE_DEPRECATED_CODE
bool	header_line_markup = true;	/* -H */
#else
bool	header_line_markup = true;	/* -Ph */
bool	ignore_case = false;		/* -PI */
bool	tokenize_html_tags = true;	/* -Pt */
#if	0
bool	separate_counts = true;		/* -Ps,-PS - default: enabled  */
#endif
bool	tokenize_html_script = false;	/* -Ps - not yet in use */
#endif

/* dual definition options */
method_t *method = NULL;

/* from html.c */
#ifdef	ENABLE_DEPRECATED_CODE
bool strict_check = false;
#endif

/* for  bogotune */
bool fBogotune = false;

/* for  robinson.c, bogoutil.c, and bogohist.c */
double	robx = 0.0;			/* used in fisher.c and rstats.c */
double	robs = 0.0;			/* used in fisher.c and rstats.c */

/* other */
uint	db_cachesize = 0;		/* in MB */
enum	passmode passmode;		/* internal */
bool	msg_count_file = false;
const char *progtype = NULL;
bool	unsure_stats = false;		/* true if print stats for unsures */

run_t run_type = RUN_UNKNOWN;
