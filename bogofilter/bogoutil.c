/* $Id$ */

/*****************************************************************************

NAME:
   bogoutil.c -- dumps and loads bogofilter text files from/to Berkeley DB format.

AUTHOR:
   Gyepi Sam <gyepi@praxis-sw.com>
   
******************************************************************************/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <db.h>

#include "datastore.h"
#include "datastore_db.h"

#undef  PROGNAME
#define PROGNAME "bogoutil"
#undef  VERSION
#define VERSION "0.2"

int verbose = 0;

int dump_file(char *db_file)
{
    dbh_t *dbh;

    DBC dbc;
    DBC *dbcp;
    DBT key, data;

    int ret;
    int rv = 0;

    dbcp = &dbc;

    memset(&key, 0, sizeof(DBT));
    memset(&data, 0, sizeof(DBT));

    if ((dbh = db_open(db_file, db_file, DB_READ)) == NULL) {
	rv = 2;
    }
    else {
	db_lock_reader(dbh);

	if ((ret = dbh->dbp->cursor(dbh->dbp, NULL, &dbcp, 0) != 0)) {
	    dbh->dbp->err(dbh->dbp, ret, PROGNAME " (cursor): %s", db_file);
	    rv = 2;
	}
	else {
	    for (;;) {
		ret = dbcp->c_get(dbcp, &key, &data, DB_NEXT);
		if (ret == 0) {
		    printf("%.*s %lu\n", key.size, (char *) key.data, *(unsigned long *) data.data);
		}
		else if (ret == DB_NOTFOUND) {
		    break;
		}
		else {
		    dbh->dbp->err(dbh->dbp, ret, PROGNAME " (c_get)");
		    rv = 2;
		    break;
		}
	    }
	}
	db_lock_release(dbh);
    }
    return rv;
}

int display_token(char *db_file, char *token)
{
    dbh_t *dbh;

    int rv = 0;

    if ((dbh = db_open(db_file, db_file, DB_READ)) == NULL) {
	rv = 2;
    }
    else {
      db_lock_reader(dbh);
      printf( "%s %d\n", token, db_getvalue( dbh, token ) );
      db_lock_release(dbh);
    }

    return rv;
}

#define BUFSIZE 512

int load_file(char *db_file)
{

    dbh_t *dbh;
    char buf[BUFSIZE];
    char *p;
    int rv = 0;
    size_t len;
    long line = 0;
    long count;

    if ((dbh = db_open(db_file, db_file, DB_WRITE)) == NULL)
	return 2;

    memset(buf, '\0', BUFSIZE);

    db_lock_writer(dbh);

    for (;;) {

	if (fgets(buf, BUFSIZE, stdin) == NULL) {
	    if (ferror(stdin)) {
		perror(PROGNAME);
		rv = 2;
	    }
	    break;
	}

	line++;

	len = strlen(buf);

	/* too short. */
	if (len < 4)
	    continue;

	p = &buf[len - 1];

	if (*(p--) != '\n') {
	    fprintf(stderr,
		    PROGNAME
		    ": Unexpected input [%s] on line %lu. Does not end with newline\n",
		    buf, line);
	    rv = 1;
	    break;
	}

	while (isdigit(*p))
	    p--;

	if (!isspace(*p)) {
	    fprintf(stderr,
		    PROGNAME
		    ": Unexpected input [%s] on line %lu. Expecting whitespace before count\n",
		    buf, line);
	    rv = 1;
	    break;
	}

	count = atol(p + 1);

	while (isspace(*p))
	    p--;

	*(p + 1) = '\0';

	/* Slower, but allows multiple lists to be concatenated */
	db_increment(dbh, buf, count);
    }
    db_lock_release(dbh);
    db_close(dbh);
    return rv;
}

void version(void)
{
    fprintf(stderr,
	    PROGNAME ": version: " VERSION "\n"
	    "Copyright (C) 2002 Gyepi Sam\n\n"
	    PROGNAME " comes with ABSOLUTELY NO WARRANTY.\n"
	    "This is free software, and you are welcome to redistribute\n"
	    "it under the General Public License.\n"
	    "See the COPYING file with the source distribution for details.\n\n");
}

void usage(void)
{
    fprintf(stderr, "Usage: %s { -d | -l } [ -v ] file.db [ -w token ] | [ -h | -V ]\n", PROGNAME);
}

void help(void)
{
    usage();
    fprintf(stderr,
	    "\t-d\tDump data from file.db to stdout.\n"
	    "\t-l\tLoad data from stdin into file.db.\n"
	    "\t-w\tDisplay a token and its count.\n"
	    "\t-v\tOutput debug messages.\n"
	    "\t-h\tPrint this message.\n"
	    "\t-V\tPrint program version.\n"
	    PROGNAME " is part of the bogofilter package.\n");
}

int main(int argc, char *argv[])
{
    typedef enum { NONE, DUMP = 1, LOAD = 2, WORD = 3 } cmd_t;

    int  count = 0;
    char *db_file, *token;
    char ch;
    cmd_t flag = NONE;

    while ((ch = getopt(argc, argv, "d:l:w:vhV")) != -1)
	switch (ch) {
	case 'd':
	    flag = DUMP;
	    count += 1;
	    db_file = (char *) optarg;
	    break;

	case 'l':
	    flag = LOAD;
	    count += 1;
	    db_file = (char *) optarg;
	    break;

	case 'w':
	    flag = WORD;
	    token = (char *) optarg;
	    break;

	case 'v':
	    verbose++;
	    break;

	case 'h':
	    help();
	    usage();
	    exit(0);

	case 'V':
	    version();
	    exit(0);

	default:
	    usage();
	    exit(1);
	}

    if ( count != 1 )
    {
      fprintf(stderr, PROGNAME ": Exactly one of the -d or -l flags must be present.\n");
      exit(1);
    }

    /* Extra or missing parameters */
    if (argc != optind) {
	usage();
	exit(1);
    }

    switch(flag)
    {
    case DUMP:
	 return dump_file(db_file);
    case LOAD:
	 return load_file(db_file);
    case WORD:
	 return display_token(db_file, token);
    }
}
