/* $Id$ */

/*****************************************************************************

NAME:
   paths.h -- prototypes and definitions for paths.c.

******************************************************************************/

#ifndef PATHS_H
#define PATHS_H

#include "common.h"

/* Path Definitions */

#if !defined(_OS2_) && !defined(__riscos__)
  #define BOGODIR ".bogofilter"
#else
  #define BOGODIR "bogofilter"
#endif

#define WORDLIST	"wordlist" DB_EXT

/* Function Prototypes */

bool build_path(char* dest, size_t size, const char* dir, const char* file);

char *build_progtype(const char *name, const char *db_type);

char *create_path_from_env(const char *var,
			   /*@null@*/ const char *subdir);

char *get_directory(priority_t which);

/** \return malloc'd copy of just the directory name of \a path */
char *get_directory_from_path(const char *path);

bool is_file(const char* path)		/*@globals errno,stderr@*/;
bool check_directory(const char *path)	/*@globals errno,stderr@*/;

#endif	/* PATHS_H */
