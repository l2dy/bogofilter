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
#include <config.h>
#ifdef HAVE_SYSLOG_H
#include <sys/syslog.h>
#endif
#include "bogofilter.h"
#include "xmalloc.h"
#include "datastore.h"
#include "xstrdup.h"

int verbose, passthrough, update, nonspam_exits_zero;

char *dirnames[] = { "BOGOFILTER_DIR", "HOME", NULL };

int main(int argc, char **argv)
{
    int	  ch;
    reg_t register_type = REG_NONE; 
    char  *directory = NULL;
    int   exitcode = 0;

    while ((ch = getopt(argc, argv, "d:ehsnSNvVpu")) != EOF)
	switch(ch)
	{
	case 'd':
	    directory = xstrdup(optarg);
	    break;

	case 'e':
	    nonspam_exits_zero = 1;
	    break;

	case 's':
	    register_type = REG_SPAM;
	    break;

	case 'n':
	    register_type = REG_GOOD;
	    break;

	case 'S':
	    register_type = REG_GOOD_TO_SPAM;
	    break;

	case 'N':
	    register_type = REG_SPAM_TO_GOOD;
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
	    update = 1;
	    break;
	}

    if ( directory == NULL )
	directory = get_bogodir(dirnames);

    setup_lists(directory, DB_WRITE);
    xfree(directory);

    if (register_type == REG_NONE)
    {
	double spamicity;
	rc_t	status = bogofilter(STDIN_FILENO, &spamicity);

	if (passthrough)
	{
	    /* print headers */
	    for (textend=&textblocks; textend->block; textend=textend->next)
	    {
		if (strcmp(textend->block, "\n") == 0) break;
		(void) fputs(textend->block, stdout);
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
		(void) fputs(textend->block, stdout);
		if (ferror(stdout)) exit(2);
	    }

	    if (fflush(stdout) || ferror(stdout)) exit(2);
	}

	exitcode = status;
	if (nonspam_exits_zero && passthrough && exitcode == 1)
	    exitcode = 0;

    } else {
	register_messages(STDIN_FILENO, register_type);
    }

    close_lists();

    exit(exitcode);
}

// End
