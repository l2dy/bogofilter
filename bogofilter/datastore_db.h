#ifndef DATASTORE_DB3_H_GUARD
#define DATASTORE_DB3_H_GUARD

#define MSG_COUNT_TOK ".MSG_COUNT"

typedef struct {
  char *filename;
  char *name;
  DB *dbp;
  pid_t pid;
} dbh_t;

extern int verbose;

#endif
