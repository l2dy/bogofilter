/* $Id$ */

/*****************************************************************************

NAME:
   bogofilter.c -- detect spam and bogons presented on standard input.

AUTHOR:
   Eric S. Raymond <esr@thyrsus.com>

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

#include "bogoconfig.h"
#include "bogofilter.h"
#include "bogoreader.h"
#include "charset.h"
#include "collect.h"
#include "datastore.h"
#include "method.h"
#include "mime.h"
#include "msgcounts.h"
#include "register.h"
#include "rstats.h"
#include "wordlists.h"

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

/* Function definitions */

/* in main.c */
extern FILE *fpo;
extern void passthrough_setup(void);
extern void passthrough_cleanup(void);
extern void write_message(rc_t status);

/* Function Prototypes */

/* Function Definitions */

static void initialize(void)
{
    mime_reset();
    token_init();
    lexer_v3_init(NULL);
    init_charset_table(charset_default, true);
}

void print_stats(FILE *fp)
{
    if (Rtable || verbose>=2)
	method->print_stats(fp);
}

rc_t bogofilter(int argc, char **argv)
{
    int msgcount = 0;
    rc_t status = RC_OK;
    token_t token_type = NONE;
    bool register_opt = run_type & (REG_SPAM | UNREG_SPAM | REG_GOOD | UNREG_GOOD);
    bool register_bef = register_opt && passthrough;
    bool register_aft = (register_opt && !passthrough) || (run_type & RUN_UPDATE);
    bool write_msg    = passthrough || Rtable || (verbose > 2);
    bool classify_msg = write_msg || (run_type & (RUN_NORMAL | RUN_UPDATE));

    wordhash_t *words = register_aft ? wordhash_init() : NULL;

    method->initialize();	/* initialize constants */

    if (query || classify_msg || write_msg) {
	set_list_active_status(true);

	if (query)
	    query_config();
    }

    bogoreader_init(argc, argv);

    while (reader_more()) {
	double spamicity;
	wordhash_t *w = wordhash_init();

	initialize();

	passthrough_setup();

	token_type = collect_words(w);
	msgcount += 1;

	if (register_opt && DEBUG_REGISTER(1))
	    fprintf(dbgout, "Message #%ld\n", (long) msgcount);
	if (register_bef)
	    register_words(run_type, w, 1);
	if (register_aft)
	    add_hash(words, w);

	if (classify_msg || write_msg) {
	    spamicity = method->compute_spamicity(w, NULL);
	    status = method->status();
	    if (run_type & RUN_UPDATE)		/* Note: don't register if RC_UNSURE */
	    {
		if (status == RC_SPAM)
		    register_words(REG_SPAM, w, msgcount);
		if (status == RC_HAM)
		    register_words(REG_GOOD, w, msgcount);
	    }

	    if (verbose && ! passthrough) {
		const char *filename = reader_filename();
		if (filename)
		    fprintf(fpo, "%s ", filename); 
	    }

	    write_message(status);
	}
	wordhash_free(w);

	rstats_cleanup();
	passthrough_cleanup();
    }

    bogoreader_fini();

    if (register_aft) {
	wordhash_sort(words);
	register_words(run_type, words, msgcount);
    }
    wordhash_free(words);

    return status;
}

/* Done */
