#include "system.h"
#include "globals.h"

/* exports */

bool nonspam_exits_zero;		/* '-e' */
bool force;				/* '-F' */
bool fisher;				/* '-f' */
bool logflag;				/* '-l' */
bool replace_nonascii_characters;	/* '-n' */
bool passthrough;			/* '-p' */
bool quiet;				/* '-q' */
bool terse;				/* '-t' */
int verbose;				/* '-v' */

FILE *fpin;				/* '-I' */

int max_repeats;
double min_dev;
double spam_cutoff;
double thresh_stats;
