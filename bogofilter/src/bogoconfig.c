/* $Id$ */

/*****************************************************************************

NAME:
   bogoconfig.c -- process config file parameters

   2003-02-12 - split out from config.c	

AUTHOR:
   David Relson <relson@osagesoftware.com>

CONTRIBUTORS:
   David Saez	-O option, helps embedding into Exim.

******************************************************************************

The call tree is (roughly):

bogoconfig.c	  process_parameters
bogoconfig.c	    process_arglist(PASS_1_CLI)
bogoconfig.c	      process_arg(PASS_1_CLI)
configfile.c	    process_config_files
configfile.c	      read_config_file
configfile.c	        process_config_option
configfile.c	          process_config_option_as_arg
bogoconfig.c	            process_arg(PASS_2_CFG)
bogoconfig.c	    process_arglist(PASS_2_CLI)
bogoconfig.c	      process_arg(PASS_3_CLI)

Note: bogolexer also uses configfile.c.
      bogolexer.c calls process_config_files(), which calls back to it.

******************************************************************************/

#include "common.h"

#include <ctype.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>

#include "bogoconfig.h"
#include "bogofilter.h"
#include "bogoreader.h"
#include "bool.h"
#include "charset.h"
#include "configfile.h"
#include "datastore.h"
#include "error.h"
#include "find_home.h"
#include "format.h"
#include "lexer.h"
#include "maint.h"
#include "paths.h"
#include "wordlists.h"
#include "wordlists_base.h"
#include "xatox.h"
#include "xmalloc.h"
#include "xstrdup.h"
#include "xstrlcpy.h"

/* includes for scoring algorithms */
#include "method.h"
#include "robinson.h"
#include "fisher.h"

#ifndef	DEBUG_CONFIG
#define DEBUG_CONFIG(level)	(verbose > level)
#endif

/*---------------------------------------------------------------------------*/

/* Global variables */

char outfname[PATH_LEN] = "";

bool  run_classify = false;
bool  run_register = false;

const char *logtag = NULL;

/* Local variables and declarations */

static int inv_terse_mode = 0;

static void display_tag_array(const char *label, FIELD *array);

static void process_arglist(int argc, char **argv, priority_t precedence, int pass);
static bool get_parsed_value(char **arg, double *parm);
static void comma_parse(char opt, const char *arg, double *parm1, double *parm2, double *parm3);

/*---------------------------------------------------------------------------*/

struct option long_options[] = {
    { "classify-files",			N, 0, 'B' },
    { "syslog-tag",			N, 0, 'L' },
    { "classify-mbox",			N, 0, 'M' },
    { "unregister-nonspam",		N, 0, 'N' },
    { "dataframe",			N, 0, 'R' },
    { "unregister-spam",		N, 0, 'S' },
    { "fixed-terse-format",		N, 0, 'T' },
    { "report-unsure",			N, 0, 'U' },
    { "version",			N, 0, 'V' },
    { "classify-stdin",			N, 0, 'b' },
    { "bogofilter_dir",			R, 0, 'd' },
    { "ham",				N, 0, 'e' },
    { "help",				N, 0, 'h' },
    { "db_cachesize",			N, 0, 'k' },
    { "use-syslog",			N, 0, 'l' },
    { "register-ham",			N, 0, 'n' },
    { "passthrough",			N, 0, 'p' },
    { "register-spam",			N, 0, 's' },
    { "terse",				N, 0, 't' },
    { "update-as-classed",		N, 0, 'u' },
    { "timestamp-date",			N, 0, 'y' },
    { "config-file",			R, 0, 'c' },
    { "no-config-file",			N, 0, 'C' },
    { "debug-flags",			R, 0, 'x' },
    { "debug-to-stdout",		N, 0, 'D' },
    { "no-header-tags",			N, 0, 'H' },
    { "input-file",			N, 0, 'I' },
    { "output-file",			N, 0, 'O' },
    { "query",				N, 0, 'Q' },
    { "verbosity",			N, 0, 'v' },
    { "block_on_subnets",		R, 0, O_BLOCK_ON_SUBNETS },
    { "charset_default",		R, 0, O_CHARSET_DEFAULT },
    { "ham_cutoff",			R, 0, O_HAM_CUTOFF },
    { "header_format",			R, 0, O_HEADER_FORMAT },
    { "log_header_format",		R, 0, O_LOG_HEADER_FORMAT },
    { "log_update_format",		R, 0, O_LOG_UPDATE_FORMAT },
    { "min_dev",			R, 0, O_MIN_DEV },
    { "replace_nonascii_characters",	R, 0, O_REPLACE_NONASCII_CHARACTERS },
    { "robs",				R, 0, O_ROBS },
    { "robx",				R, 0, O_ROBX },
    { "spam_cutoff",			R, 0, O_SPAM_CUTOFF },
    { "spam_header_name",		R, 0, O_SPAM_HEADER_NAME },
    { "spam_subject_tag",		R, 0, O_SPAM_SUBJECT_TAG },
    { "spamicity_formats",		R, 0, O_SPAMICITY_FORMATS },
    { "spamicity_tags",			R, 0, O_SPAMICITY_TAGS },
    { "stats_in_header",		R, 0, O_STATS_IN_HEADER },
    { "terse",				R, 0, O_TERSE },
    { "terse_format",			R, 0, O_TERSE_FORMAT },
    { "thresh_update",			R, 0, O_THRESH_UPDATE },
    { "timestamp",			R, 0, O_TIMESTAMP },
    { "unsure_subject_tag",		R, 0, O_UNSURE_SUBJECT_TAG },
    { "user_config_file",		R, 0, O_USER_CONFIG_FILE },
    { NULL,				0, 0, 0 }
};

static bool get_bool(const char *name, const char *arg)
{
    bool b = str_to_bool(arg);
    if (DEBUG_CONFIG(2))
	fprintf(dbgout, "%s -> %s\n", name,
		b ? "Yes" : "No");
    return b;
}

static bool get_double(const char *name, const char *arg, double *d)
{
    remove_comment(arg);
    if (!xatof(d, arg))
	return false;
    if (DEBUG_CONFIG(2))
	fprintf(dbgout, "%s -> %f\n", name, *d);
    return true;
}

static char *get_string(const char *name, const char *arg)
{
    char *s = xstrdup(arg);
    if (DEBUG_CONFIG(2))
	fprintf(dbgout, "%s -> '%s'\n", name, s);
    return s;
}

static void set_bogofilter_dir(const char *name, const char *arg, int precedence)
{
    char *dir = tildeexpand(arg, true);
    if (DEBUG_CONFIG(2))
	fprintf(dbgout, "%s -> '%s'\n", name, dir);
    if (setup_wordlists(dir, precedence) != 0)
	exit(EX_ERROR);
    xfree(dir);
    return;
}

void process_parameters(int argc, char **argv, bool warn_on_error)
{
    bogotest = 0;
    verbose = 0;
    run_type = RUN_UNKNOWN;
    fpin = stdin;
    dbgout = stderr;
    set_today();		/* compute current date for token age */

    method = (method_t *) &rf_fisher_method;
    usr_parms = method->config_parms;

    process_arglist(argc, argv, PR_COMMAND, PASS_1_CLI);
    process_config_files(warn_on_error);
    process_arglist(argc, argv, PR_COMMAND, PASS_3_CLI);

    /* directories from command line and config file are already handled */

    if (setup_wordlists(NULL, PR_ENV_BOGO) != 0 &&
	setup_wordlists(NULL, PR_ENV_HOME) != 0)
	exit(EX_ERROR);

    stats_prefix= stats_in_header ? "  " : "# ";

    if (DEBUG_CONFIG(0))
	fprintf(dbgout, "stats_prefix: '%s'\n", stats_prefix);

    return;
}

static bool get_parsed_value(char **arg, double *parm)
{
    char *str = *arg;
    bool ok = true;
    if (parm && str && *str) {
	if (*str == ',')
	    str += 1;
	else {
	    ok = xatof(parm, str);
	    str = strchr(str+1, ',');
	    if (str)
		str += 1;
	}
	*arg = str;
    }
    return ok;
}

void comma_parse(char opt, const char *arg, double *parm1, double *parm2, double *parm3)
{
    char *parse = xstrdup(arg);
    char *copy = parse;
    bool ok = ( get_parsed_value(&copy, parm1) &&
		get_parsed_value(&copy, parm2) &&
		get_parsed_value(&copy, parm3) );
    if (!ok)
	fprintf(stderr, "Cannot parse -%c option argument '%s'.\n", opt, arg);
    xfree(parse);
}

static run_t check_run_type(run_t add_type, run_t conflict)
{
    if (run_type & conflict) {
	(void)fprintf(stderr, "Error:  Invalid combination of options.\n");
	exit(EX_ERROR);
    }
    return (run_type | add_type);
}

static int validate_args(void)
{
/*  flags '-s', '-n', '-S', and '-N' are mutually exclusive with
    flags '-p', '-u', '-e', and '-R'. */
    run_classify = (run_type & (RUN_NORMAL | RUN_UPDATE)) != 0;
    run_register = (run_type & (REG_SPAM | REG_GOOD | UNREG_SPAM | UNREG_GOOD)) != 0;

    if (*outfname && !passthrough)
    {
	(void)fprintf(stderr,
		      "Warning: Option -O %s has no effect without -p\n",
		      outfname);
    }
    
    if (run_register && (run_classify || (Rtable != 0)))
    {
	(void)fprintf(stderr,
		      "Error:  Option '-u' may not be used with options '-s', '-n', '-S', or '-N'.\n"
	    );
	return EX_ERROR;
    }

    return EX_OK;
}

typedef struct {
    const char *shortname;
    const char *longname;
    parm_t	type;
    union
    {
	void	*v;
	func	*f;
	bool	*b;
	int	*i;
	double	*d;
	char	*c;
	char   **s;
    } addr;
} option_names_t;

/*
option_names_t option_names[] = {
    { "-h",  "--help",					  CP_FUNCTION,	{ (void *) &help } },
    { "-V",  "--version",				  CP_FUNCTION,	{ (void *) &print_version } },
    { "-Q",  "--query",					  CP_FUNCTION,	{ (void *) &query } },
    { "-p",  "--passthrough",				  CP_TRUE,	{ (void *) &passthrough } },
    { "-e",  "--ham-true",				  CP_TRUE,	{ (void *) &nonspam_exits_zero } },
    { "-u",  "--update-as-classed",			  CP_FUNCTION,	{ (void *) &run_update } },
    { "-M",  "--classify-mbox",				  CP_TRUE,	{ (void *) &mbox_mode } },
    { "-b",  "--classify-stdin",			  CP_FUNCTION,	{ (void *) &set_stdin } },
    { "-B",  "--classify-files=nCme1,name2,...",	  CP_FUNCTION,	{ (void *) &set_filenames } },
    { "-R",  "--dataframe",				  CP_FUNCTION,	{ (void *) &set_dataframe } },
    { "-s",  "--register-spam",				  CP_FUNCTION,	{ (void *) &reg_spam } },
    { "-n",  "--register-ham",				  CP_FUNCTION,	{ (void *) &reg_good } },
    { "-S",  "--unregister-spam",			  CP_FUNCTION,	{ (void *) &unreg_spam } },
    { "-N",  "--unregister-nonspam",			  CP_FUNCTION,	{ (void *) &unreg_good } },
    { "-c",  "--config-file=file",			  CP_FUNCTION,	{ (void *) &set_configfile } },
    { "-C",  "--no-config-file",			  CP_FUNCTION,	{ (void *) &set_no_configfile } },
    { "-d",  "--bogofilter_dir=path",			  CP_STRING,	{ (void *) &path } },
    { "-H",  "--no-header-tags",			  CP_FALSE,	{ (void *) &header_line_markup } },
    { "-k",  "--db_cachesize=size",			  CP_INTEGER,	{ (void *) &size } },
    { "-l",  "--use-syslog",				  CP_TRUE,	{ (void *) &syslog } },
    { "-L",  "--syslog-tag=tag",			  CP_STRING,	{ (void *) &tag } },
    { "-I",  "--input-file=file",			  CP_FUNCTION,	{ (void *) &bogoreader_name } },
    { "-O",  "--output-file=file",			  CP_FUNCTION,	{ (void *) &set_outfname } },
    { "-m v1[,v2[,v3]]"," --min_dev=v1, --robs=v2, --robx=v3", CP_NONE,	{ (void *) NULL } },
    { "-o v1[,v2]",     "--spam_cutoff=v1, --ham_cutoff=v2",   CP_NONE,	{ (void *) NULL } },
    { "-t",  "--terse",					  CP_IMMD_FUNC,	{ (void *) &set_terse } },
    { "-T",  "--fixed-terse-format",			  CP_IMMD_FUNC,	{ (void *) &set_inv_terse_mode } },
    { "-U",  "--report-unsure=bool",			  CP_BOOLEAN,	{ (void *) &unsure_stats } },
    { "-v",  "--verbosity",				  CP_IMMD_FUNC,	{ (void *) &increment_verbosity } },
    { "-y",  "--timestamp-date",			  CP_IMMD_FUNC,	{ (void *) &set_string_date } },
    { "-D",  "--debug-to-stdout",			  CP_FUNCTION,	{ (void *) &set_dbgout } },
    { "-x list",  "--debug-flags=list",			  CP_IMMD_FUNC,	{ (void *) &set_debug_mask } }
};
*/

static const char *help_text[] = {
    "help options:\n",
    "  -h,                       - print this help message.\n",
    "  -V, --version             - print version information and exit.\n",
    "  -Q, --query               - query (display) bogofilter configuration.\n",
    "classification options:\n",
    "  -p, --passthrough         - passthrough.\n",
    "  -e, --ham-true            - in -p mode, exit with code 0 when the mail is not spam.\n",
    "  -u, --update-as-classified- classify message as spam or non-spam and register accordingly.\n",
    "  -M, --clasify-mbox        - set mailbox mode.  Classify multiple messages in an mbox formatted file.\n",
    "  -b, --clasify-stdin       - set streaming bulk mode. Process multiple messages whose filenames are read from STDIN.\n",
    "  -B, --classify-files=list - set bulk mode. Process multiple messages named as files on the command line.\n",
    "  -R, --dataframe           - print an R data frame.\n",
    "registration options:\n",
    "  -s, --register-spam       - register message(s) as spam.\n",
    "  -n, --register-ham        - register message(s) as non-spam.\n",
    "  -S, --unregister-spam     - unregister message(s) from spam list.\n",
    "  -N, --unregister-nonspam  - unregister message(s) from non-spam list.\n",
    "general options:\n",
    "  -c, --config-file=file    - read specified config file.\n",
    "  -C, --no-config-file      - don't read standard config files.\n",
    "  -d, --bogofilter_dir=path - specify directory for wordlists.\n",
    "  -H, --no-header-tags      - disables header line tagging.\n",
    "  -k, --db_cachesize=size   - set BerkeleyDB cache size (MB).\n",
    "  -l, --use-syslog          - write messages to syslog.\n",
    "  -L, --syslog-tag=tag      - specify the tag value for log messages.\n",
    "  -I, --input-file=file     - read message from 'file' instead of stdin.\n",
    "  -O, --output-file=file    - save message to 'file' in passthrough mode.\n",
    "parameter options:\n",
    "  -m,  --min_dev=v1, --robs=2, --robx=v3v1[,v2[,v3]]\n",
    "                            - set user defined min_dev, robs, and robx values.\n",
    "  -o, --spam_cutoff=v1, --ha_cutoff=v2v1[,v2]\n",
    "                           - set user defined spam and non-spam cutoff values.\n",
    "info options:\n",
    "  -t, --terse               - set terse output mode.\n",
    "  -T, --fixed-terse-format  - set invariant terse output mode.\n",
    "  -U, --report-unsure       - print statistics if spamicity is 'unsure'.\n",
    "  -v, --verbosity           - set debug verbosity level.\n",
    "  -y, --timestamp-date      - set date for token timestamps.\n",
    "  -D, --debug-to-stdout     - direct debug output to stdout.\n",
    "  -x, --debug-flags=list    - set flags to display debug information.\n",
    "config file options:\n",
    "  --option=value - can be used to set the value of a config file option.\n",
    "                   see bogofilter.cf.example for more info.\n",
    "  --block_on_subnets                return class addr tokens\n",
    "  --bogofilter_dir                  directory for wordlists\n",
    "  --charset_default                 default character set\n",
    "  --db_cachesize                    Berkeley db cache in Mb\n",
    "  --degen_enabled                   if no match, simplify\n",
    "  --first_match                     if degen use first match\n",
    "  --ham_cutoff                      nonspam if score below\n",
    "  --header_format                   spam header format\n",
    "  --log_header_format               header written to log\n",
    "  --log_update_format               logged on update\n",
    "  --min_dev                         ignore if score near\n",
    "  --replace_nonascii_characters     substitute '?' if bit8 is 1\n",
    "  --robs                            Robinson's s parameter\n",
    "  --robx                            Robinson's x\n",
    "  --spam_cutoff                     spam if score above this\n",
    "  --spam_header_name                passthrough adds/replaces\n",
    "  --spam_subject_tag                passthrough prepends Subject\n",
    "  --spamicity_formats               spamicity output format\n",
    "  --spamicity_tags                  spamicity tag format\n",
    "  --stats_in_header                 use header not body\n",
    "  --terse                           report in short form\n",
    "  --terse_format                    short form\n",
    "  --thresh_update                   no update if near 0 or 1\n",
    "  --timestamp                       apply token timestamps\n",
    "  --unsure_subject_tag              like spam_subject_tag\n",
    "  --user_config_file                configuration file\n",
    "\n",
    "bogofilter is a tool for classifying email as spam or non-spam.\n",
    "\n",
    "For updates and additional information, see\n",
    "URL: http://bogofilter.sourceforge.net\n",
    NULL
};

static void help(void)
{
    uint i;
    (void)fprintf(stderr,
                  "%s version %s\n"
                  "\n"
                  "Usage:  %s [options] < message\n",
                  progtype, version, PACKAGE
	);
    for (i=0; help_text[i] != NULL; i++)
	(void)fprintf(stderr, "%s", help_text[i]);
}

static void print_version(void)
{
    (void)fprintf(stderr,
		  "%s version %s\n"
		  "    Database: %s\n"
		  "Copyright (C) 2002-2004 Eric S. Raymond,\n"
		  "David Relson, Matthias Andree, Greg Louis\n\n"
		  "%s comes with ABSOLUTELY NO WARRANTY.  "
		  "This is free software, and\nyou are welcome to "
		  "redistribute it under the General Public License.  "
		  "See\nthe COPYING file with the source distribution for "
		  "\n", 
		  progtype, version, ds_version_str(), PACKAGE);
}

#define	OPTIONS	":-:bBc:Cd:DefFghHI:k:lL:m:MnNo:O:pqQRrsStTuUvVx:X:y:"

/** These functions process command line arguments.
 **
 ** They are called to perform passes 1 & 2 of command line switch processing.
 ** The config file is read in between the two function calls.
 **
 ** The functions will exit if there's an error, for example if
 ** there are leftover command line arguments.
 */

static void process_arglist(int argc, char **argv, priority_t precedence, int pass)
{
    int option;
    ex_t exitcode;

    if (pass != PASS_1_CLI) {
	optind = opterr = 1;
	/* don't use #ifdef here: */
#if HAVE_DECL_OPTRESET
	optreset = 1;
#endif
    }

    while (1) 
    {
	int option_index = 0;
	int this_option_optind = optind ? optind : 1;
	const char *name;

	option = getopt_long(argc, argv, OPTIONS,
			     long_options, &option_index);

	if (option == -1)
	    break;

	name = (option_index == 0) ? argv[this_option_optind] : long_options[option_index].name;

#if 0
	if (getenv("BOGOFILTER_DEBUG_OPTIONS")) {
	    fprintf(stderr, "config: option=%c (%d), optind=%d, opterr=%d, optarg=%s\n", 
		    isprint((unsigned char)option) ? option : '_', option, 
		    optind, opterr, optarg ? optarg : "(null)");
	}
#endif

	process_arg(option, name, optarg, precedence, pass);
    }

    if (pass == PASS_1_CLI) {
	if (run_type == RUN_UNKNOWN)
	    run_type = RUN_NORMAL;
    }

    if (pass == PASS_3_CLI) {
	exitcode = validate_args();
	if (exitcode) 
	    exit(exitcode);

	if (bulk_mode == B_NORMAL && optind < argc) {
	    fprintf(stderr, "Extra arguments given, first: %s. Aborting.\n", argv[optind]);
	    exit(EX_ERROR);
	}

	if (inv_terse_mode) {
	    verbose = max(1, verbose); 	/* force printing */
	    set_terse_mode_format(inv_terse_mode);
	}
    }

    return;
}

void process_arg(int option, const char *name, const char *val, priority_t precedence, arg_pass_t pass)
{
    switch (option)
    {
    case 'b':
	bulk_mode = B_STDIN;
	fpin = NULL;	/* Ensure that input file isn't stdin */
	break;

    case 'B':
	bulk_mode = B_CMDLINE;
	break;

    case 'c':
	if (pass == PASS_1_CLI)
	    read_config_file(val, false, !quiet, PR_CFG_USER);

	/*@fallthrough@*/
	/* fall through to suppress reading config files */

    case 'C':
	if (pass == PASS_1_CLI)
	    suppress_config_file = true;
	break;

    case O_USER_CONFIG_FILE:
	user_config_file = get_string(name, val);
	break;

    case 'D':
	dbgout = stdout;
	break;

    case 'e':
	nonspam_exits_zero = true;
	break;

    case 'h':
	help();
	exit(EX_OK);

    case 'I':
	if (pass == PASS_1_CLI)
	    bogoreader_name(val);
	break;

    case 'L':
	logtag = val;
	/*@fallthrough@*/

    case 'l':
	logflag = true;
	break;

    case 'M':
	mbox_mode = true;
	break;

    case 'n':
	run_type = check_run_type(REG_GOOD, REG_SPAM | UNREG_GOOD);
	break;

    case 'N':
	run_type = check_run_type(UNREG_GOOD, REG_GOOD | UNREG_SPAM);
	break;

    case 'O':
	if (pass == PASS_1_CLI)
	    xstrlcpy(outfname, val, sizeof(outfname));
	break;

    case 'p':
	passthrough = true;
	break;

    case 'Q':
	query = true;
	break;

    case 'R':
	Rtable = 1;
	break;

    case 's':
	run_type = check_run_type(REG_SPAM, REG_GOOD | UNREG_SPAM);
	break;

    case 'S':
	run_type = check_run_type(UNREG_SPAM, REG_SPAM | UNREG_GOOD);
	break;

    case 'u':
	run_type |= RUN_UPDATE;
	break;
	
    case 'U':
	unsure_stats = true;
	break;

    case 'v':
	if (pass == PASS_1_CLI)
	    verbose++;
	break;

    case 'x':
	set_debug_mask( val );
	break;

    case 'X':
	set_bogotest( val );
	break;

    case 'y':		/* date as YYYYMMDD */
	set_date( string_to_date(val) );
	break;

    case ':':
	fprintf(stderr, "Option -%c requires an argument.\n", optopt);
	exit(EX_ERROR);

    case '?':
	fprintf(stderr, "Unknown option '%s'.\n", name);
	exit(EX_ERROR);

    case '-':
	if (pass == PASS_3_CLI)
	    process_config_option(val, true, precedence);
	break;

    case 'd':
	if (pass != PASS_1_CLI)
	    set_bogofilter_dir(WORDLIST, val, precedence);
	break;

    case 'H':
	header_line_markup = false;
	break;

    case 'k':
	db_cachesize=atoi(val);
	break;

    case 'm':
	if (pass != PASS_1_CLI) {
	    comma_parse(option, val, &min_dev, &robs, &robx);
	    if (DEBUG_CONFIG(1))
		printf("md %6.4f, rs %6.4f, rx %6.4f\n", min_dev, robs, robx);
	}
	break;

    case O_MIN_DEV:
	if (pass != PASS_1_CLI)
	    get_double(name, val, &min_dev);
	break;

    case O_ROBS:
	if (pass != PASS_1_CLI)
	    get_double(name, val, &robs);
	break;

    case O_ROBX:
	if (pass != PASS_1_CLI)
	    get_double(name, val, &robx);
	break;

    case 'o':
	if (pass != PASS_1_CLI) {
	    comma_parse(option, val, &spam_cutoff, &ham_cutoff, NULL);
	    if (DEBUG_CONFIG(1))
		printf("sc %6.4f, hc %6.4f\n", spam_cutoff, ham_cutoff);
	}
	break;

    case O_SPAM_CUTOFF:
	if (pass != PASS_1_CLI)
	    get_double(name, val, &spam_cutoff);
	break;

    case O_HAM_CUTOFF:
	if (pass != PASS_1_CLI)
	    get_double(name, val, &ham_cutoff);
	break;

    case 't':
	terse = true;
	break;

    case 'T':			/* invariant terse mode */
	terse = true;
	if (pass == PASS_1_CLI)
	    inv_terse_mode += 1;
	break;

    case 'V':
	print_version();
	exit(EX_OK);
	
    case O_BLOCK_ON_SUBNETS:		block_on_subnets = get_bool(name, val);			break;
    case O_CHARSET_DEFAULT:		charset_default = get_string(name, val);		break;
    case O_HEADER_FORMAT:		header_format = get_string(name, val);			break;
    case O_LOG_HEADER_FORMAT:		log_header_format = get_string(name, val);		break;
    case O_LOG_UPDATE_FORMAT:		log_update_format = get_string(name, val);		break;
    case O_REPLACE_NONASCII_CHARACTERS:	replace_nonascii_characters = get_bool(name, val);	break;
    case O_SPAMICITY_FORMATS:		set_spamicity_formats(val);				break;
    case O_SPAMICITY_TAGS:		set_spamicity_tags(val);				break;
    case O_SPAM_HEADER_NAME:		spam_header_name = get_string(name, val);		break;
    case O_SPAM_SUBJECT_TAG:		spam_subject_tag = get_string(name, val);		break;
    case O_STATS_IN_HEADER:		stats_in_header = get_bool(name, val);			break;
    case O_TERSE:			terse = get_bool(name, val);				break;	
    case O_TERSE_FORMAT:		terse_format = get_string(name, val);			break;
    case O_THRESH_UPDATE:		get_double(name, val, &thresh_update);			break;
    case O_TIMESTAMP:			timestamp_tokens = get_bool(name, val);			break;
    case O_UNSURE_SUBJECT_TAG:		unsure_subject_tag = get_string(name, val);		break;
    }
}

#define YN(b) (b ? "yes" : "no")

void query_config(void)
{
    fprintf(stdout, "%s version %s\n", progname, version);
    fprintf(stdout, "\n");
    fprintf(stdout, "%-11s = %s\n",            "algorithm", method->name);
    fprintf(stdout, "%-11s = %0.6f (%8.2e)\n", "robx", robx, robx);
    fprintf(stdout, "%-11s = %0.6f (%8.2e)\n", "robs", robs, robs);
    fprintf(stdout, "%-11s = %0.6f (%8.2e)\n", "min_dev", min_dev, min_dev);
    fprintf(stdout, "%-11s = %0.6f (%8.2e)\n", "ham_cutoff", ham_cutoff, ham_cutoff);
    fprintf(stdout, "%-11s = %0.6f (%8.2e)\n", "spam_cutoff", spam_cutoff, spam_cutoff);
    fprintf(stdout, "\n");
    fprintf(stdout, "%-17s = %s\n", "block_on_subnets",		YN(block_on_subnets));
    fprintf(stdout, "%-17s = %s\n", "replace_nonascii_characters", YN(replace_nonascii_characters));
    fprintf(stdout, "\n");
    fprintf(stdout, "%-17s = '%s'\n", "spam_header_name",  spam_header_name);
    fprintf(stdout, "%-17s = '%s'\n", "header_format",     header_format);
    fprintf(stdout, "%-17s = '%s'\n", "terse_format",      terse_format);
    fprintf(stdout, "%-17s = '%s'\n", "log_header_format", log_header_format);
    fprintf(stdout, "%-17s = '%s'\n", "log_update_format", log_update_format);
    display_tag_array("spamicity_tags   ", spamicity_tags);
    display_tag_array("spamicity_formats", spamicity_formats);
    exit(EX_OK);
}

static void display_tag_array(const char *label, FIELD *array)
{
    int i;
    int count = (ham_cutoff < EPS) ? 2 : 3;
    const char *s;

    fprintf(stdout, "%s =", label);
    for (i = 0, s = ""; i < count; i += 1, s = ",")
	fprintf(stdout, "%s '%s'", s, array[i]);
    fprintf(stdout, "\n");
}
