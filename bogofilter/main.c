/* $Id$ */

/*****************************************************************************

NAME:
   main.c -- detect spam and bogons presented on standard input.

AUTHOR:
   Eric S. Raymond <esr@thyrsus.com>

******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/stat.h>
#include <errno.h>
#include <config.h>
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif
#include "version.h"
#include "common.h"
#include "bogofilter.h"

#define BOGODIR ".bogofilter"

int verbose, passthrough, nonspam_exits_zero;
int Rtable = 0;
FILE *Rfp;
enum algorithm algorithm = AL_GRAHAM;

char directory[PATH_LEN] = "";

int  logflag;
char msg_register[80];
char msg_bogofilter[80];

const char *progname = "bogofilter";

run_t run_type = RUN_NORMAL; 

static char  *Rfn = NULL;

/* if the given environment variable 'var' exists, copy it to 'dest' and
   tack on the optional 'subdir' value.
 */
static void set_dir_from_env(char* dest, const char *var, const char* subdir)
{
    char *env;
    int path_left=PATH_LEN-1;

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
static int check_directory(const char* path)
{
    int rc;
    struct stat sb;

    rc = stat(path, &sb);
    if (rc < 0) {
	if (ENOENT==errno) {
	    if(mkdir(path, S_IRUSR|S_IWUSR|S_IXUSR)) {
		perror("Error creating directory");
		return -1;
	    }
	    else if (verbose > 0) {
		fprintf(stderr, "Created directory %s .\n", path);
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
	    fprintf(stderr, "Error: %s is not a directory.\n", path);
	}
    }
    return 0;
}

int validate_args(int argc, char **argv)
{
    bool registration, classification;

//  flags '-s', '-n', '-S', or '-N', are mutually exclusive of flags '-p', '-u', '-e', and '-R'.
    classification = (run_type == RUN_NORMAL) ||(run_type == RUN_UPDATE) || passthrough || nonspam_exits_zero || (Rtable != 0);
    registration   = (run_type = REG_SPAM) || (run_type = REG_GOOD) || (run_type = REG_GOOD_TO_SPAM) || (run_type = REG_SPAM_TO_GOOD);

    if ( registration && classification)
    {
	fprintf(stderr, "Error:  Invalid combination of options.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "    Options '-s', '-n', '-S', and '-N' are used when registering words.\n");
	fprintf(stderr, "    Options '-p', '-u', '-e', and '-R' are used when classifying messages.\n");
	fprintf(stderr, "    The two sets of options may not be used together.\n");
	fprintf(stderr, "    \n");
//	fprintf(stderr, "    Options '-g', '-r', '-l', '-d', '-x', and '-v' may be used with either mode.\n");
	return 2;
    }

    return 0;
}

int process_args(int argc, char **argv)
{
    int	  ch;
    int   exitcode;
    int	  optind_save;

    optind_save=1;
    while ((ch = getopt(argc, argv, "d:ehlsnSNvVpugR::rx:")) != EOF)
    {
	switch(ch)
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
	    printf( "\n" );
	    printf( "Usage: bogofilter [options] < message\n" );
	    printf( "\t-h\t- print this help message.\n" );
	    printf( "\t-d path\t- specify directory for wordlists.\n" );
	    printf( "\t-p\t- passthrough.\n" );
	    printf( "\t-e\t- in -p mode, exit with code 0 when the mail is not spam.\n");
	    printf( "\t-s\t- register message as spam.\n" );
	    printf( "\t-n\t- register message as non-spam.\n" );
	    printf( "\t-S\t- move message's words from non-spam list to spam list.\n" );
	    printf( "\t-N\t- move message's words from spam list to spam non-list.\n" );
	    printf( "\t-v\t- set debug verbosity level.\n" );
	    printf( "\t-V\t- print version info.\n" );
	    printf( "\n" );
	    printf( "bogofilter is a tool for classifying email as spam or non-spam.\n" );
	    printf( "\n" );
	    printf( "For updates and additional information, see\n" );
	    printf( "URL: http://bogofilter.sourceforge.net\n" );
	    printf( "\n" );
	    exit(0);

        case 'V':
            printf("\n%s version %s ", PACKAGE, VERSION);
            printf("Copyright (C) 2002 Eric S. Raymond\n\n");
            printf("%s comes with ABSOLUTELY NO WARRANTY. ", PACKAGE);
            printf("This is free software, and you\nare welcome to ");
            printf("redistribute it under the General Public License. ");
            printf("See the\nCOPYING file with the source distribution for ");
            printf("details.\n\n");
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
	    char *tmp;
	    Rtable = 1;
	    tmp = (optind_save == optind) ? argv[optind] : optarg;
	    Rfn = tmp ? strdup(tmp) : NULL;
	    if (Rfn != NULL) {
	        if(!(Rfp = fopen(Rfn, "w"))) {
	            fprintf(stderr, "Error: can't write %s\n", Rfn);
	            Rtable = 0;
	        }
	    } else Rfp = stdout;
	}
	    // fall through to force Robinson calculations
	case 'r':
	    algorithm = AL_ROBINSON;
	    break;

	case 'x':
	    set_debug_mask( argv[optind] );
	    break;
	}
	optind_save = optind+1;
    }

    exitcode = validate_args(argc, argv);

    return exitcode;
}

int main(int argc, char **argv)
{
    int   exitcode;

    set_dir_from_env(directory, "HOME", BOGODIR);
    set_dir_from_env(directory, "BOGOFILTER_DIR", NULL);

    exitcode = process_args(argc, argv);
    if (exitcode != 0)
	exit(exitcode);

    if (!directory[0]) {
	fprintf(stderr, "%s: cannot find bogofilter directory.\n"
		"You must set the BOGOFILTER_DIR or HOME environment variables\n"
		"or use the -d DIR option. The program aborts.\n", progname);
	exit(2);
    }
    if (check_directory(directory)) exit(2);

    setup_lists(directory);

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
			if (strcmp(textend->block, "\n") == 0) break;
			(void) fwrite(textend->block, 1, textend->len, stdout);
			if (ferror(stdout)) exit(2);
		    }
		}

		if (passthrough || verbose)
		{
		    /* print spam-status at the end of the header
		     * then mark the beginning of the message body */
		    printf("%s: %s, tests=bogofilter, spamicity=%0.6f, version=%s\n", 
			    SPAM_HEADER_NAME, 
			    (status==RC_SPAM) ? "Yes" : "No", 
			    spamicity, VERSION);
		    if (passthrough+verbose > 1)
		    {
			(void)fputs("\n", stdout);
			verbose += passthrough;
			print_bogostats( stdout );
			verbose -= passthrough;
		    }
		}

		if (passthrough)
		{
		    /* If the message terminated early (without body or blank
		     * line between header and body), enforce a blank line to
		     * prevent anything past us from choking. */
		    if (!textend->block)
			(void)fputs("\n", stdout);

		    /* print body */
		    for (; textend->block; textend=textend->next)
		    {
			(void) fwrite(textend->block, 1, textend->len, stdout);
			if (ferror(stdout)) exit(2);
		    }

		    if (fflush(stdout) || ferror(stdout)) exit(2);
		}

		exitcode = status;
		if (nonspam_exits_zero && passthrough && exitcode == 1)
		    exitcode = 0;

		sprintf(msg_bogofilter, "%s: %s, spamicity=%0.6f, version=%s",
			SPAM_HEADER_NAME, (status==RC_SPAM) ? "Yes" : "No", spamicity, VERSION);
	    }
	    break;
	default:
	    register_messages(STDIN_FILENO, run_type);
	    break;
    }
    
    if(Rtable && Rfp != stdout && fclose(Rfp) != 0)
	fprintf(stderr, "Error: couldn't close %s\n", Rfn);

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
	    syslog(LOG_INFO, msg_register);
	    break;
	}

	closelog();
    }
#endif

    exit(exitcode);
}

// End
