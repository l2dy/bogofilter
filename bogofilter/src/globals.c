/* $Id$ */

/*****************************************************************************

NAME:
   globals.c -- define global variables

AUTHOR:
   Matthias Andree <matthias.andree@gmx.de>

******************************************************************************/

#include "system.h"
#include "globals.h"
#include "score.h"

/* exports */

/* command line options */

bulk_t	bulk_mode = B_NORMAL;		/* '-b, -B' */
bool	suppress_config_file;		/* '-C' */
bool	nonspam_exits_zero;		/* '-e' */
FILE	*fpin = NULL;			/* '-I' */
bool	logflag;			/* '-l' */
bool	mbox_mode;			/* '-M' */
bool	replace_nonascii_characters;	/* '-n' */
bool	passthrough;			/* '-p' */
bool	quiet;				/* '-q' */
int	query;				/* '-Q' */
bool	Rtable;				/* '-R' */
bool	terse;				/* '-t' */
int	bogotest;			/* '-X', env("BOGOTEST") */
int	verbose;			/* '-v' */

/* config file options */
double	min_dev;
double	ham_cutoff = HAM_CUTOFF;
double	spam_cutoff;
double	thresh_update;

const char	*update_dir;
/*@observer@*/
const char	*stats_prefix;

/* for lexer_v3.l */
bool	header_line_markup = true;	/* -H */

/* for  transactions */
#ifndef	ENABLE_TRANSACTIONS
bool fTransaction = false;
#else
bool fTransaction = true;
#endif

/* for  bogotune */
bool 	fBogotune = false;

/* for  bogoconfig.c, prob.c, rstats.c and score.c */
double	robx = 0.0;
double	robs = 0.0;
double	sp_esf = SP_ESF;
double	ns_esf = NS_ESF;

/* other */
FILE	*fpo;
uint	db_cachesize = DB_CACHESIZE;	/* in MB */
enum	passmode passmode;		/* internal */
bool	msg_count_file = false;
char	*progtype = NULL;
bool	unsure_stats = false;		/* true if print stats for unsures */

run_t run_type = RUN_UNKNOWN;

uint	wordlist_version;
