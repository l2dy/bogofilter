/* $Id$ */
/* $Log$
 * Revision 1.2  2002/09/15 16:31:41  relson
 * Substitute STDIN_FILENO where numeric constant 0 is used as a file descriptor.
 *
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
#include "bogofilter.h"
#ifdef HAVE_SYSLOG_H
#include <sys/syslog.h>
#endif

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

    while ((ch = getopt(argc, argv, "d:shSHvpl")) != EOF)
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

	case 'p':
	    passthrough++;
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
	int	status = bogofilter(STDIN_FILENO);

	if (passthrough)
	{
	    int	inheaders = 1;

	    for (textend=&textblocks; textend->block; textend=textend->next)
	    {
		if (inheaders == 0) {
			/* print message body lines without further checks */
			(void) fputs(textend->block, stdout);
		}
		else if (strncmp(textend->block, "X-Spam-Status:", 14) == 0) {
			/* Don't print existing X-Spam-Status: headers */
			#ifdef HAVE_SYSLOG_H
			syslog(LOG_INFO, "Ignored exising X-Spam-Status header");
			#endif
		}
		else if (strncmp(textend->block, "Subject:", 8) == 0) {
			/* Append the X-Spam-Status: header before subject */
			printf("X-Spam-Status: %s, tests=bogofilter\n", 
			status ? "No" : "Yes");
			(void) fputs(textend->block, stdout);
		}
		else if (strcmp(textend->block, "\n") == 0) {
			/* Mark the beginning of the message body */
			inheaders = 0;
			(void) fputs(textend->block, stdout);
		}
		else {
			/* in case none of the above conditions were met */
			(void) fputs(textend->block, stdout);
		}
	    }

	}
	exit(status);
    }

    exit(0);
}

// End
