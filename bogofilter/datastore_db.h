/* $Id$ */

#ifndef DATASTORE_DB3_H_GUARD
#define DATASTORE_DB3_H_GUARD

#define MSG_COUNT_TOK ".MSG_COUNT"

typedef struct {
  char *filename;
  char *name;
  DB *dbp;
  pid_t pid;
  bool locked;
  int  is_swapped;
} dbh_t;

#endif
