/*****************************************************************************

NAME:
   bogofilter.c -- detect spam and bogons presented on standard input.

AUTHORS:
   Eric S. Raymond <esr@thyrsus.com>
   David Relson    <relson@osagesoftware.com>
   Matthias Andree <matthias.andree@gmx.de>
   Greg Louis      <glouis@dynamicro.on.ca>

THEORY:

   Originally implemented as Paul Graham's variant of Bayes filtering,
   as described in 

     "A Plan For Spam", http://www.paulgraham.com/spam.html

   Updated in accordance with Gary Robinson's proposed modifications,
   as described at

    http://radio.weblogs.com/0101454/stories/2002/09/16/spamDetection.html

******************************************************************************/

#include "common.h"

#include <string.h>
#include <stdlib.h>

#include "bogofilter.h"
#include "bogoconfig.h"
#include "bogoreader.h"
#include "collect.h"
#include "format.h"
#include "passthrough.h"
#include "register.h"
#include "rstats.h"
#include "score.h"

/*
**	case B_NORMAL:		
**	case B_STDIN:		* '-b' - streaming (stdin) mode *
**	case B_CMDLINE:		* '-B' - command line mode *
**
**loop:
**    read & parse a message
**	if -p, save textblocks
**    register if -snSN && -pe
**    classify if -pue && ! -snSN
**    register if -u
**    write    if -p
**    if (-snSN && -pe) || -u
**	free tokens
**    else
**	accumulate tokens	
**
**end:	register if -snSN && ! -pe
*/

/* Function Definitions */

void print_stats(FILE *fp)
{
    msg_print_stats(fp);
}

rc_t bogofilter(int argc, char **argv)
{
    uint msgcount = 0;
    rc_t status = RC_OK;
    bool register_opt = (run_type & (REG_SPAM | UNREG_SPAM | REG_GOOD | UNREG_GOOD)) != 0;
    bool register_bef = register_opt && passthrough;
    bool register_aft = ((register_opt && !passthrough) || (run_type & RUN_UPDATE)) != 0;
    bool write_msg    = passthrough || Rtable;
    bool classify_msg = write_msg || ((run_type & (RUN_NORMAL | RUN_UPDATE))) != 0;

    wordhash_t *words;

    score_initialize();			/* initialize constants */

    if (query)
	return query_config();

    words = register_aft ? wordhash_new() : NULL;

    bogoreader_init(argc, (const char * const *) argv);

    while ((*reader_more)()) {
	wordhash_t *w = wordhash_new();

	rstats_init();
	passthrough_setup();

	collect_words(w);
	wordhash_sort(w);
	msgcount += 1;

	format_set_counts(w->count, msgcount);

        if (!passthrough_keepopen())
            bogoreader_close_ifeof();
        
	if (register_opt && DEBUG_REGISTER(1))
	    fprintf(dbgout, "Message #%ld\n", (long) msgcount);
	if (register_bef)
	    register_words(run_type, w, 1);
	if (register_aft)
	    wordhash_add(words, w, &wordprop_init);

	if (classify_msg || write_msg) {
	    double spamicity;
	    lookup_words(w);			/* This reads the database */
	    spamicity = msg_compute_spamicity(w);
	    status = msg_status();
	    if (run_type & RUN_UPDATE)		/* Note: don't register if RC_UNSURE */
	    {
		if (status == RC_SPAM && spamicity <= 1.0 - thresh_update)
		    register_words(REG_SPAM, w, msgcount);
		if (status == RC_HAM && spamicity >= thresh_update)
		    register_words(REG_GOOD, w, msgcount);
	    }

	    if (verbose && !passthrough && !quiet) {
		const char *filename = (*reader_filename)();
		if (filename)
		    fprintf(fpo, "%s ", filename); 
	    }

	    write_message(status);		/* passthrough */
	    if (logflag && !register_opt) {
		write_log_message(status);
		msgcount = 0;
	    }
	}
	wordhash_free(w);

	passthrough_cleanup();
	rstats_cleanup();

	if (DEBUG_MEMORY(2))
	    MEMDISPLAY;

	if (fDie)
	    exit(EX_ERROR);
    }

    bogoreader_fini();

    if (DEBUG_MEMORY(1))
	MEMDISPLAY;

    if (register_aft && ((run_type & RUN_UPDATE) == 0)) {
	wordhash_sort(words);
	register_words(run_type, words, msgcount);
    }

    score_cleanup();

    if (logflag && register_opt)
	write_log_message(status);

    wordhash_free(words);

    if (DEBUG_MEMORY(1))
	MEMDISPLAY;

    return status;
}

/* Done */
