/* $Id$ */
/* $Log$
 * Revision 1.7  2002/09/23 10:08:49  m-a
 * Integrate patch by Zeph Hull and Clint Adams to present spamicity in
 * X-Spam-Status header in bogofilter -p mode.
 *
/* Revision 1.6  2002/09/20 15:27:27  m-a
/* Optimize bogofilter -p: use tighter loop, print our X-Spam-Status: header after
/* all other headers, and ensure that there is always an empty line after the
/* headers. We have much less checks in the loops, so it should be somewhat
/* faster now.
/*
/* Revision 1.5  2002/09/17 07:19:04  adrian_otto
/* Added -V mode for printing out version information.
/*
/* Revision 1.4  2002/09/16 20:44:13  m-a
/* Do not increment passthrough on -p, but set it to 1.
/*
/* Revision 1.3  2002/09/15 19:07:13  relson
/* Add an enumerated type for return codes of RC_SPAM and RC_NONSPAM, which  values of 0 and 1 as called for by procmail.
/* Use the new codes and type for bogofilter() and when generating the X-Spam-Status message.
/*
/* Revision 1.2  2002/09/15 16:31:41  relson
/* Substitute STDIN_FILENO where numeric constant 0 is used as a file descriptor.
/*
/* Revision 1.1.1.1  2002/09/14 22:15:20  adrian_otto
/* 0.7.3 Base Source
/* */
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
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <config.h>
#ifdef HAVE_SYSLOG_H
#include <sys/syslog.h>
#endif
#include "bogofilter.h"

#define BOGODIR		"/.bogofilter/"
#define HAMFILE		"hamlist.db"
#define SPAMFILE	"spamlist.db"

#define HAMCOUNTFILE	"hamlist.count"
#define SPAMCOUNTFILE	"spamlist.count"

int verbose, passthrough;

int main(int argc, char **argv)
{
    int	ch, dump = 0;
    int register_spam = 0, register_ham = 0;
    int spam_to_ham = 0, ham_to_spam = 0;
    char	hamfile[PATH_MAX], spamfile[PATH_MAX], directory[PATH_MAX];
    char	hamcountfile[PATH_MAX], spamcountfile[PATH_MAX];
    char	*tmp;
    struct stat sb;
    int readerror=0;

    if ( (tmp = getenv("HOME")) != NULL ) {
    	strcpy(directory, tmp );
    }
    strcat(directory, BOGODIR);

    while ((ch = getopt(argc, argv, "d:shSHvVpl")) != EOF)
	switch(ch)
	{
	case 'd':
	    strcpy(directory, optarg);
	   if (directory[strlen(directory)-1] != '/')
               strcat(directory, "/" );
	    break;

	case 's':
	    register_spam = 1;
	    break;

	case 'h':
	    register_ham = 1;
	    break;

	case 'S':
	    ham_to_spam = 1;
	    break;

	case 'H':
	    spam_to_ham = 1;
	    break;

	case 'v':
	    verbose++;
	    break;

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

	case 'l':
	    dump = 1;
	    break;
	}


    ch = stat(directory, &sb);
    if (!((ch == 0 && S_ISDIR(sb.st_mode))
	  || (errno==ENOENT && mkdir(directory, S_IRUSR|S_IWUSR|S_IXUSR)==0)))
    {
	fprintf(stderr,"bogofilter: something is wrong with %s.\n", directory);
	exit(2);
    }

    strcpy(hamfile, directory);
    strcat(hamfile, HAMFILE);
    ham_list.file = hamfile;

    strcpy(spamfile, directory);
    strcat(spamfile, SPAMFILE);
    spam_list.file = spamfile;

    strcpy(hamcountfile, directory);
    strcat(hamcountfile, HAMCOUNTFILE);
    ham_list.count_file = hamcountfile;

    strcpy(spamcountfile, directory);
    strcat(spamcountfile, SPAMCOUNTFILE);
    spam_list.count_file = spamcountfile;

    readerror += read_list(&ham_list);
    readerror += read_list(&spam_list);

    /* readerror is ok, but only if using register_* modes */
    if (readerror && ! (register_spam || register_ham)) {
	fprintf(stderr, "Error: can't open list file(s).\n");
	exit(2);
    }

    if (dump)
    {
	if (register_ham)
	    bogodump(hamfile);
	else if (register_spam)
	    bogodump(spamfile);
    }
    else if (register_spam)
    {
	register_words(STDIN_FILENO, &spam_list, NULL);
	write_list(&spam_list);
	if (verbose)
	    printf("bogofilter: %lu messages on the spam list\n", spam_list.msgcount);
    }
    else if (register_ham)
    {
	register_words(STDIN_FILENO, &ham_list, NULL);
	write_list(&ham_list);
	if (verbose)
	    printf("bogofilter: %lu messages on the ham list\n", ham_list.msgcount);
    }
    else if (spam_to_ham)
    {
	register_words(STDIN_FILENO, &ham_list, &spam_list);
	write_list(&ham_list);
	write_list(&spam_list);
    }
    else if (ham_to_spam)
    {
	register_words(STDIN_FILENO, &spam_list, &ham_list);
	write_list(&ham_list);
	write_list(&spam_list);
    }
    else
    {
	double spamicity;
	rc_t	status = bogofilter(STDIN_FILENO, &spamicity);

	if (passthrough)
	{
	    int	inheaders = 1;

	    /* print headers */
	    for (textend=&textblocks; textend->block; textend=textend->next)
	    {
		if (strcmp(textend->block, "\n") == 0) break;
		(void) fputs(textend->block, stdout);
	    }

	    /* print spam-status at the end of the header
	     * then mark the beginning of the message body */
	    printf("X-Spam-Status: %s, tests=bogofilter, spamicity=%0.6f\n", 
		    (status==RC_SPAM) ? "Yes" : "No", spamicity);
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
	exit(status);
    }

    exit(0);
}

// End
