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

#define PROGNAME "bogoutil"
#define VERSION "0.1"

int  verbose = 0;

int
dump_file (char *file)
{
  int ret;
  DB db;
  DB *dbp;
  DBC dbc;
  DBC *dbcp;
  DBT key, data;

  dbp = &db;
  dbcp = &dbc;

  if ((ret = db_create (&dbp, NULL, 0)) != 0)
    {
      fprintf (stderr, PROGNAME " (db_create): %s\n", db_strerror (ret));
      return 1;
    }

  if ((ret = dbp->open (dbp, file, NULL, DB_BTREE, 0, 0)) != 0)
    {
      dbp->err (dbp, ret, PROGNAME " (open): %s", file);
      return 1;
    }

  if ((ret = dbp->cursor (dbp, NULL, &dbcp, 0) != 0))
    {
      dbp->err (dbp, ret, PROGNAME " (cursor): %s", file);
      return 1;
    }

  memset (&key, 0, sizeof (DBT));
  memset (&data, 0, sizeof (DBT));

  for (;;)
    {
      ret = dbcp->c_get (dbcp, &key, &data, DB_NEXT);
      if (ret == 0)
	{
	  printf ("%.*s %lu\n", key.size, (char *) key.data,
		  *(unsigned long *) data.data);
	}
      else if (ret == DB_NOTFOUND)
	{
	  break;
	}
      else
	{
	  dbp->err (dbp, ret, PROGNAME " (c_get)");
	  break;
	}
    }
  return 0;
}

#define BUFSIZE 512

int
load_file(char *db_file){

  dbh_t *dbh;
  char buf[BUFSIZE];
  char *p;
  int rv = 0;
  size_t len;
  long line = 0;
  long count;

  if ( (dbh = db_open(db_file, db_file)) == NULL) return 2;

  memset(buf, '\0', BUFSIZE);

  for (;;) {

    if (fgets(buf, BUFSIZE, stdin) == NULL){
      if (ferror(stdin)){
        perror(PROGNAME);
        rv = 2;
      }
      break;
    }

    line++;

    len = strlen(buf);

    /* too short.*/
    if (len < 4) continue;

    p = &buf[len - 1];

    if (*(p--)  != '\n'){
      fprintf(stderr, PROGNAME ": Unexpected input [%s] on line %lu. Does not end with newline\n", buf, line);
      rv = 1;
      break;
    }

    while(isdigit(*p)) p--;

    if (!isspace(*p)){
      fprintf(stderr, PROGNAME ": Unexpected input [%s] on line %lu. Expecting space before count\n", buf, line);
      rv = 1;
      break;
    }
    
    count = atol(p + 1);

    while(isspace(*p)) p--;

    *(p+1) = '\0';

    /* Slower, but allows multiple lists to be concatenated */
    db_increment(dbh, buf, count);
  }

  db_close(dbh);
  return rv;
}

void
version (void)
{
  fprintf (stderr, PROGNAME ": version: " VERSION "\n");
  exit (0);
}

void
usage (void)
{
  fprintf (stderr, "Usage: %s -d | -l [ -V ] [ -h ] file.db\n", PROGNAME);
  exit (1);
}

void
help (void)
{
  fprintf (stderr, "Help: %s\n", PROGNAME);
  fprintf (stderr, "Read UPGRADE for help\n");
  exit (0);
}

void
ensure_uniq_flag (int flag)
{
  if (flag != 0)
    {
      fprintf (stderr, PROGNAME ": Flags -d and -l are mutually exclusive.\n");
      exit (2);
    }
}

int
main (int argc, char *argv[])
{
  enum { DUMP = 1, LOAD = 2 };

  char *db_file;
  int ch;
  int flag = 0;

  while ((ch = getopt (argc, argv, "dlhV")) != -1)
    switch (ch)
      {
      case 'd':
	ensure_uniq_flag (flag);
	flag = DUMP;
	break;

      case 'l':
	ensure_uniq_flag (flag);
	flag = LOAD;
	break;

      case 'h':
	help ();
	/* unreachable */
	break;

      case 'V':
	version ();
	/* unreachable */
	break;

      default:
	usage ();
	/* unreachable */
	break;
      }

  /* Extra or missing parameters */
  if ((argc - optind) != 1){
      fprintf (stderr, PROGNAME ": Exacly one filename argument is expected.\n");
      exit (2);
  }

  db_file = argv[optind++];

  if (flag == DUMP)
    {
      return dump_file (db_file);
    }
  else if (flag == LOAD)
    {
      return load_file (db_file);
    }
  else
    {
      fprintf (stderr, PROGNAME ": One of the flags -d or -l is required\n");
      exit (2);
    }
}
