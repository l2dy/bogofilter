/* $Id$ */

/*****************************************************************************

NAME:
   main.c -- detect spam and bogons presented on standard input.

AUTHOR:
   Eric S. Raymond <esr@thyrsus.com>

******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <config.h>

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif

#include "version.h"
#include "common.h"
#include "lexer.h"
#include "bogofilter.h"
#include "bogoconfig.h"

#define BOGODIR ".bogofilter"

int verbose, passthrough, nonspam_exits_zero;
int Rtable = 0;
/*@null@*/ /*@dependent@*/ FILE *Rfp;
algorithm_t algorithm = AL_GRAHAM;

char directory[PATH_LEN] = "";

int  logflag;
char msg_register[1024];
char msg_bogofilter[1024];

const char *progname = "bogofilter";
char *system_config_file = "/etc/bogofilter.cf";
char *user_config_file   = ".bogofilter.cf";

char *spam_header_name = SPAM_HEADER_NAME;

run_t run_type = RUN_NORMAL; 

int thresh_index = 0;
double thresh_stats = 0.0f;
double thresh_rtable = 0.0f;

/* if the given environment variable 'var' exists, copy it to 'dest' and
   tack on the optional 'subdir' value.
 */
static void set_dir_from_env(/*@reldef@*/ /*@unique@*/ char* dest,
	const char *var,
	/*@null@*/ const char *subdir)
{
    char *env;
    size_t path_left=PATH_LEN-1;

    env = getenv(var);
    if (env == NULL) return;

    strncpy(dest, env, path_left-1);  // leave one char left for '/'
    path_left -= strlen(env);
    if ('/' != dest[strlen(dest)-1]) {
	strcat(dest, "/");
	path_left--;
    }
    if (subdir && (path_left > 0)) {
	strncat(dest, subdir, path_left);
    }
}

/* check that our directory exists and try to create it if it doesn't
   return -1 on failure, 0 otherwise.
 */
static int check_directory(const char* path) /*@globals errno,stderr@*/
{
    int rc;
    struct stat sb;

    if (*path == '\0') {
	(void)fprintf(stderr, "%s: cannot find bogofilter directory.\n"
		"You must set the BOGOFILTER_DIR or HOME environment variables\n"
		"or use the -d DIR option. The program aborts.\n", progname);
	return 0;
    }

    rc = stat(path, &sb);
    if (rc < 0) {
	if (ENOENT==errno) {
	    if(mkdir(path, S_IRUSR|S_IWUSR|S_IXUSR)) {
		perror("Error creating directory");
		return -1;
	    }
	    else if (verbose > 0) {
		(void)fprintf(stderr, "Created directory %s .\n", path);
	    }
	    return 0;
	}
	else {
	    perror("Error accessing directory");
	    return -1;
	}
    }
    else {
	if (! S_ISDIR(sb.st_mode)) {
	    (void)fprintf(stderr, "Error: %s is not a directory.\n", path);
	}
    }
    return 0;
}

static int validate_args(/*@unused@*/ int argc, /*@unused@*/ char **argv)
{
    bool registration, classification;

//  flags '-s', '-n', '-S', or '-N', are mutually exclusive of flags '-p', '-u', '-e', and '-R'.
    classification = (run_type == RUN_NORMAL) ||(run_type == RUN_UPDATE) || passthrough || nonspam_exits_zero || (Rtable != 0);
    registration   = (run_type == REG_SPAM) || (run_type == REG_GOOD) || (run_type == REG_GOOD_TO_SPAM) || (run_type == REG_SPAM_TO_GOOD);

    if ( registration && classification)
    {
	(void)fprintf(stderr, "Error:  Invalid combination of options.\n");
	(void)fprintf(stderr, "\n");
	(void)fprintf(stderr, "    Options '-s', '-n', '-S', and '-N' are used when registering words.\n");
	(void)fprintf(stderr, "    Options '-p', '-u', '-e', and '-R' are used when classifying messages.\n");
	(void)fprintf(stderr, "    The two sets of options may not be used together.\n");
	(void)fprintf(stderr, "    \n");
//	(void)fprintf(stderr, "    Options '-g', '-r', '-l', '-d', '-x', and '-v' may be used with either mode.\n");
	return 2;
    }

    return 0;
}

static int quell_config_read = 0;

static int process_args(int argc, char **argv)
{
    int option;
    int exitcode;

    while ((option = getopt(argc, argv, "d:ehlsnSNvVpugqR:rx:")) != EOF)
    {
	switch(option)
	{
	case 'd':
	    strncpy(directory, optarg, PATH_LEN);
	    break;

	case 'e':
	    nonspam_exits_zero = 1;
	    break;

	case 's':
	    run_type = REG_SPAM;
	    break;

	case 'n':
	    run_type = REG_GOOD;
	    break;

	case 'S':
	    run_type = REG_GOOD_TO_SPAM;
	    break;

	case 'N':
	    run_type = REG_SPAM_TO_GOOD;
	    break;

	case 'v':
	    verbose++;
	    break;

	case 'h':
	    (void)printf( "\n" );
	    (void)printf( "Usage: bogofilter [options] < message\n" );
	    (void)printf( "\t-h\t- print this help message and exit.\n" );
	    (void)printf( "\t-d path\t- specify directory for wordlists.\n" );
	    (void)printf( "\t-p\t- passthrough.\n" );
	    (void)printf( "\t-e\t- in -p mode, exit with code 0 when the mail is not spam.\n");
	    (void)printf( "\t-s\t- register message as spam.\n" );
	    (void)printf( "\t-n\t- register message as non-spam.\n" );
	    (void)printf( "\t-S\t- move message's words from non-spam list to spam list.\n" );
	    (void)printf( "\t-N\t- move message's words from spam list to spam non-list.\n" );
	    (void)printf( "\t-v\t- set verbosity level.\n" );
	    (void)printf( "\t-x LIST\t- set debug flags.\n" );
	    (void)printf( "\t-V\t- print version information and exit.\n" );
	    (void)printf( "\t-q\t- prevent reading configuration files.\n" );
	    (void)printf( "\n" );
	    (void)printf( "bogofilter is a tool for classifying email as spam or non-spam.\n" );
	    (void)printf( "\n" );
	    (void)printf( "For updates and additional information, see\n" );
	    (void)printf( "URL: http://bogofilter.sourceforge.net\n" );
	    (void)printf( "\n" );
	    exit(0);

        case 'V':
            (void)printf("\n%s version %s ", PACKAGE, VERSION);
            (void)printf("Copyright (C) 2002 Eric S. Raymond\n\n");
            (void)printf("%s comes with ABSOLUTELY NO WARRANTY. ", PACKAGE);
            (void)printf("This is free software, and you\nare welcome to ");
            (void)printf("redistribute it under the General Public License. ");
            (void)printf("See the\nCOPYING file with the source distribution for ");
            (void)printf("details.\n\n");
            exit(0);

	case 'p':
	    passthrough = 1;
	    break;

	case 'u':
	    run_type = RUN_UPDATE;
	    break;

	case 'l':
	    logflag = 1;
	    break;

	case 'g':
	    algorithm = AL_GRAHAM;
	    break;

	case 'R':
	{
	    Rtable = 1;
	    Rfp = fopen(optarg, "w");
	    if(Rfp == NULL) {
		(void)fprintf(stderr, "Error: can't write %s\n", optarg);
		Rtable = 0;
	    }
	}
	/*@fallthrough@*/
	/* fall through to force Robinson calculations */
	case 'r':
	    algorithm = AL_ROBINSON;
	    break;

	case 'x':
	    set_debug_mask( optarg );
	    break;

	case 'q':
	    quell_config_read = 1;
	    break;
	}
    }

    exitcode = validate_args(argc, argv);

    return exitcode;
}

int main(int argc, char **argv) /*@globals errno,stderr,stdout@*/
{
    int   exitcode;

    set_dir_from_env(directory, "HOME", BOGODIR);
    set_dir_from_env(directory, "BOGOFILTER_DIR", NULL);

    exitcode = process_args(argc, argv);
    if (exitcode != 0)
	exit(exitcode);

    if (check_directory(directory))
	exit(2);

    if (setup_lists(directory, GOOD_BIAS, 1.0))
	exit(2);

    if (!quell_config_read) {
	read_config_file(system_config_file, FALSE);
	read_config_file(user_config_file, TRUE);
    }

    switch(run_type) {
	case RUN_NORMAL:
	case RUN_UPDATE:
	    {
		double spamicity;
		rc_t   status = bogofilter(STDIN_FILENO, &spamicity);

		if (passthrough)
		{
		    /* print headers */
		    for (textend=&textblocks; textend->block; textend=textend->next)
		    {
			if (textend->len == 1
			       && memcmp(textend->block, "\n", 1) == 0)
			    break;

			(void) fwrite(textend->block, 1, textend->len, stdout);
			if (ferror(stdout)) exit(2);
		    }
		}

		if (passthrough || verbose)
		{
		    /* print spam-status at the end of the header
		     * then mark the beginning of the message body */
		    (void)printf("%s: %s, tests=bogofilter, spamicity=%0.6f, version=%s\n", 
			    spam_header_name, 
			    (status==RC_SPAM) ? "Yes" : "No", 
			    spamicity, VERSION);
		}

		if (verbose || passthrough || Rtable)
		{
		    // (void)fputs("\n", stdout);
		    verbose += passthrough;
		    print_bogostats( stdout, spamicity );
		    verbose -= passthrough;
		}

		if (passthrough)
		{
		    /* If the message terminated early (without body or blank
		     * line between header and body), enforce a blank line to
		     * prevent anything past us from choking. */
		    if (!textend->block)
			(void)fputs("\n", stdout);

		    /* print body */
		    for (; textend->block != NULL; textend=textend->next)
		    {
			(void) fwrite(textend->block, 1, textend->len, stdout);
			if (ferror(stdout)) exit(2);
		    }

		    if (fflush(stdout) || ferror(stdout)) exit(2);
		}

		exitcode = status;
		if (nonspam_exits_zero && passthrough && exitcode == 1)
		    exitcode = 0;

		(void)sprintf(msg_bogofilter, "%s: %s, spamicity=%0.6f, version=%s",
			spam_header_name, (status==RC_SPAM) ? "Yes" : "No",
			spamicity, VERSION);
	    }
	    break;
	default:
	    register_messages(STDIN_FILENO, run_type);
	    break;
    }
    
    if(Rtable && Rfp != NULL && fclose(Rfp) != 0)
	(void)fprintf(stderr, "Error: couldn't close Rtable file\n");

    close_lists();

#ifdef HAVE_SYSLOG_H
    if (logflag)
    {
	openlog( "bogofilter", LOG_PID, LOG_DAEMON );

	switch (run_type)
	{
	case RUN_NORMAL:
	    syslog(LOG_INFO, "%s\n", msg_bogofilter);
	    break;
	case RUN_UPDATE:
	    syslog(LOG_INFO, "%s, %s\n", msg_bogofilter, msg_register);
	    break;
	default:
	    syslog(LOG_INFO, "%s", msg_register);
	    break;
	}

	closelog();
    }
#endif

    exit(exitcode);
}

// End
