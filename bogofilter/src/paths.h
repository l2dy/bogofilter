/* $Id$ */

/**
 * \file
 * paths.h -- prototypes and definitions for paths.c.
 */

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

/** Build a path to a file given a directory and file name in malloc()d
 * memory (caller freed), concatenating dir and file, adding a slash if
 * necessary.  \return
 * - true for success
 * - false for error (esp. overflow)
 */
char *build_path(const char* dir, const char* file);

char *build_progtype(const char *name, const char *db_type);

/** If the given environment variable \a var exists, create a path from
 * it and tack on the optional \a subdir value.
 * \return
 * - buffer address if success
 * - NULL if failure
 */
char *create_path_from_env(const char *var,
			   /*@null@*/ const char *subdir);

char *get_directory(priority_t which);

/** \return malloc'd copy of just the file name of \a path */
char *get_file_from_path(const char *path);

/** \return malloc'd copy of just the directory name of \a path */
char *get_directory_from_path(const char *path);

/** Check whether \a path is a file (everything that is not a directory
 * or a symlink to a directory).
 * \return
 * - true if \a path is a file
 * - false if \a path is a directory or an error occurred */
bool is_file(const char* path)		/*@globals errno,stderr@*/;

/** Check that directory \a path exists and try to create it otherwise.
 * \return
 * - true on success
 * - false on error */
bool check_directory(const char *path)	/*@globals errno,stderr@*/;

#endif	/* PATHS_H */
