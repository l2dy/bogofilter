/* $Id$ */

/*****************************************************************************

NAME:
   paths.h -- prototypes for paths.c.

******************************************************************************/

#ifndef PATHS_H
#define PATHS_H

int build_path(char* dest, size_t size, const char* dir, const char* file);

char *build_progtype(const char *name, const char *db_type);

char *create_path_from_env(const char *var,
			   /*@null@*/ const char *subdir);

char *get_directory(priority_t which);

int check_directory(const char *path) /*@globals errno,stderr@*/;

#endif	/* PATHS_H */
