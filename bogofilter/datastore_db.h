/* $Id$ */

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

#endif
