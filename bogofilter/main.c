/* $Id$ */
/*
 * $Log$
 * Revision 1.22  2002/10/04 18:40:17  relson
 * If the HOME path already contains 'bogofilter', use it without tacking on BOGODIR.
 *
 * Revision 1.21  2002/10/04 18:08:25  relson
 * Added a '-u' (update) option so that the appropriate wordlist, i.e. spamlist.db
 * or goodlist.db,  is updated after classifying the message.
 *
 * Revision 1.20  2002/10/04 04:29:20  relson
 * Clean up allocation and deallocation of directory.
 *
 * Revision 1.19  2002/10/04 03:26:52  gyepi
 * bug fix: make sure directory points to heap memory when it is used
 *
 * Revision 1.18  2002/10/04 01:35:23  m-a
 * Free directory if used.
 *
 * Revision 1.17  2002/10/02 17:14:54  relson
 * main.c now calls setup_lists() for initializing the wordlist structures, including the opening of the wordlist.db files.
 * setup_list() takes a directory name as its argument and passes it to init_list(), which calls open_wordlist() for the actual open.
 *
 * Revision 1.16  2002/10/02 16:27:40  relson
 * Initial inclusion of multiple wordlist code into bogofilter.
 *
 * Revision 1.15  2002/09/29 03:40:54  gyepi
 *
 * Modified: bogofilter.c bogofilter.h main.c
 * 1. replace Judy with hash table (wordhash)
 * 2. ensure that databases are always locked in the same order.
 *
 * Apologies for simultaneously submitting loosely related changes.
 *
 * Revision 1.14  2002/09/26 23:04:40  relson
 * documentation:
 *     changed to refer to "good" and "spam" tokens and lists.
 *     removed '-l' option as this function is now in bogoutil.
 *
 * filenames:
 *     changed database from "hamlist.db" to "goodlist.db".
 *
 * variables:
 *     renamed "ham_list" and "hamness" to "good_list" and "goodness".
 *
 * Revision 1.13  2002/09/25 18:17:09  relson
 * Added '-h' option to print help message and exit.
 *
 * Revision 1.12  2002/09/25 18:15:19  relson
 * Renamed '-h' and '-H' options to '-n' and '-N' in main.c.
 * Modified documentation to show the new names.
 *
 * Revision 1.11  2002/09/24 04:34:19  gyepi
 *
 *
 *  Modified Files:
 *  	Makefile.am  -- add entries for datastore* + and other new files
 *         bogofilter.c bogofilter.h main.c -- fixup to use database abstraction
 *
 *  Added Files:
 *  	datastore_db.c datastore_db.h datastore.h -- database abstraction. Also implements locking
 * 	xmalloc.c xmalloc.h -- utility
 *  	bogoutil.c  -- dump/restore utility.
 *
 * 1. Implements database abstraction as discussed.
 *    Also implements multiple readers/single writer file locking.
 *
 * 2. Adds utility to dump/restore databases.
 *
 * Revision 1.10  2002/09/23 11:34:30  relson
 * Modify passthrough code so that X-Spam-Status line will also print in verbose mode.
 *
 * Revision 1.9  2002/09/23 11:31:53  m-a
 * Unnest comments, and move $ line down by one to prevent CVS from adding nested comments again.
 *
 * Revision 1.8  2002/09/23 11:27:25  m-a
 * Drop unused `inheaders' variable, unnest comments.
 *
 * Revision 1.7  2002/09/23 10:08:49  m-a
 * Integrate patch by Zeph Hull and Clint Adams to present spamicity in
 * X-Spam-Status header in bogofilter -p mode.
 *
 * Revision 1.6  2002/09/20 15:27:27  m-a
 * Optimize bogofilter -p: use tighter loop, print our X-Spam-Status: header after
 * all other headers, and ensure that there is always an empty line after the
 * headers. We have much less checks in the loops, so it should be somewhat
 * faster now.
 *
 * Revision 1.5  2002/09/17 07:19:04  adrian_otto
 * Added -V mode for printing out version information.
 *
 * Revision 1.4  2002/09/16 20:44:13  m-a
 * Do not increment passthrough on -p, but set it to 1.
 *
 * Revision 1.3  2002/09/15 19:07:13  relson
 * Add an enumerated type for return codes of RC_SPAM and RC_NONSPAM, which  values of 0 and 1 as called for by procmail.
 * Use the new codes and type for bogofilter() and when generating the X-Spam-Status message.
 *
 * Revision 1.2  2002/09/15 16:31:41  relson
 * Substitute STDIN_FILENO where numeric constant 0 is used as a file descriptor.
 *
 * Revision 1.1.1.1  2002/09/14 22:15:20  adrian_otto
 * 0.7.3 Base Source
 * */
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
#include "xstrdup.h"
#include "datastore.h"

#define BOGODIR		"/.bogofilter/"

int verbose, passthrough, update;

int main(int argc, char **argv)
{
    int	  ch;
    reg_t register_type = REG_NONE; 
    char  *directory = NULL;
    int   exitcode = 0;

    while ((ch = getopt(argc, argv, "d:hsnSNvVpu")) != EOF)
	switch(ch)
	{
	case 'd':
	    directory = optarg;
	    setup_lists(directory);
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
    {
	char *tmp = getenv("HOME");
	if ( tmp == NULL ) {
	    setup_lists(BOGODIR);
	} else {
	    directory = xmalloc( strlen(tmp) + strlen(BOGODIR) + 1 );
	    strcpy(directory, tmp );
	    if ( strstr(tmp, "bogofilter") == NULL )
		strcat(directory, BOGODIR);
	    setup_lists(directory);
	    xfree(directory);
	}
    }

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
	    }
	}

	if (passthrough || verbose)
	{
	    /* print spam-status at the end of the header
	     * then mark the beginning of the message body */
	    printf("X-Spam-Status: %s, tests=bogofilter, spamicity=%0.6f\n", 
		    (status==RC_SPAM) ? "Yes" : "No", spamicity);
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
	    }
	}

	exitcode = status;
    }
    else
    {
	register_messages(STDIN_FILENO, register_type);
    }

    close_lists();

    exit(exitcode);
}

// End
