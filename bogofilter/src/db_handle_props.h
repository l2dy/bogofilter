/* $Id$ */

/*****************************************************************************

NAME:
   db_handle_props.h -- fragments of code common to datastore_t?db.c

******************************************************************************/

/* Get the current process id */
unsigned long db_handle_pid(void *vhandle){
  dbh_t *handle = vhandle;
  return handle->pid;
}

/* Get the database filename */
char *db_handle_filename(void *vhandle){
  dbh_t *handle = vhandle;
  return handle->filename;
}

