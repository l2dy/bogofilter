/* $Id$ */

#ifndef DATASTORE_H
#define DATASTORE_H

#define MSG_COUNT_TOK ".MSG_COUNT"

typedef struct {
  char *filename;
  char *name;
  DB *dbp;
  pid_t pid;
  bool locked;
  int  is_swapped;
} dbh_t;

#endif	/* DATASTORE_H */
