/* $Id$ */

/*****************************************************************************

NAME:
   db_handle_props.h -- fragments of code common to datastore_t?db.c

******************************************************************************/

/* Get the database filename */
char *db_handle_filename(void *vhandle){
  dbh_t *handle = vhandle;
  return handle->filename;
}

