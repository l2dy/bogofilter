/* $Id$ */
/*
 * $Log$
 * Revision 1.3  2002/10/04 04:01:51  relson
 * Added cvs keywords Id and Log to the files' headers.
 *
 */


#ifndef DATASTORE_DB3_H_GUARD
#define DATASTORE_DB3_H_GUARD

#include "common.h"

#define MSG_COUNT_TOK ".MSG_COUNT"

typedef struct {
  char *filename;
  char *name;
  DB *dbp;
  pid_t pid;
  bool locked;

} dbh_t;

extern int verbose;

#endif
