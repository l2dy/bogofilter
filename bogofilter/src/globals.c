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
int	max_repeats=1;			/* set default value */
double	min_dev;
double	ham_cutoff = HAM_CUTOFF;
double	spam_cutoff;
double	thresh_update;

const char	*update_dir;
/*@observer@*/
const char	*stats_prefix;

/* for lexer_v3.l */
bool	header_line_markup = true;	/* -H */

/* for  bogotune */
bool fBogotune = false;

/* for  robinson.c, bogoutil.c, and bogohist.c */
double	robx = 0.0;			/* used in fisher.c and rstats.c */
double	robs = 0.0;			/* used in fisher.c and rstats.c */

/* other */
FILE	*fpo;
uint	db_cachesize = 0;		/* in MB */
enum	passmode passmode;		/* internal */
bool	msg_count_file = false;
const char *progtype = NULL;
bool	unsure_stats = false;		/* true if print stats for unsures */

run_t run_type = RUN_UNKNOWN;
