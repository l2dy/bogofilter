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

bool	twostate;			/* '-2' */
bool	threestate;			/* '-3' */
bulk_t	bulk_mode = B_NORMAL;		/* '-b, -B' */
bool	suppress_config_file;		/* '-C' */
bool	nonspam_exits_zero;		/* '-e' */
bool	force;				/* '-F' */
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
int	test = 0;			/* '-T' */
int	verbose;			/* '-v' */
wl_t	wordlists = W_SEPARATE;		/* '-w', '-W' */

/* config file options */
int	max_repeats;
double	min_dev;
double	spam_cutoff;
double	thresh_stats;

const char	*update_dir;
/*@observer@*/
const char	*stats_prefix;

/* for lexer_v3.l */
bool	ignore_case = false;		/* -PI */
bool	header_line_markup = true;	/* -Ph */
bool	tokenize_html_tags = true;	/* -Pt */
bool	tokenize_html_script = false;	/* -Ps - not yet in use */

/* dual definition options */
char	*directory;			/* '-d' */
method_t *method = NULL;

/* other */
int	db_cachesize = 0;		/* in MB */
enum	passmode passmode;		/* internal */
bool	msg_count_file = false;
