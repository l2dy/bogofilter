/* $Id$ */

#ifndef DATASTORE_DB_H
#define DATASTORE_DB_H

#define MSG_COUNT_TOK ".MSG_COUNT"

typedef struct {
  char *filename;
  char *name;
  DB *dbp;
  pid_t pid;
  bool locked;
  int  is_swapped;
} dbh_t;

typedef struct {
    long count;
    long date;
} dbv_t;

void db_setvalue_and_date(void *vhandle, const char * word, long value, long date);

#endif	/* DATASTORE_H */
