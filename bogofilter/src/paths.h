/* $Id$ */

/*****************************************************************************

NAME:
   paths.h -- prototypes and definitions for paths.c.

******************************************************************************/

#ifndef PATHS_H
#define PATHS_H

/* Path Definitions */

#define BOGODIR ".bogofilter"

#ifndef __OS2__
  #define BOGODIR ".bogofilter"
  #define SLASH   '/'
  #define SLASH_STR "/"
#else
  #define BOGODIR "bogofilter"
  #define SLASH   '\\'
  #define SLASH_STR "\\"
#endif

#define WORDLIST	"wordlist" DB_EXT
#define GOODFILE	"goodlist" DB_EXT
#define SPAMFILE	"spamlist" DB_EXT
#define IGNOREFILE	"ignorelist" DB_EXT

/* Function Prototypes */

int build_path(char* dest, size_t size, const char* dir, const char* file);

char *build_progtype(const char *name, const char *db_type);

char *create_path_from_env(const char *var,
			   /*@null@*/ const char *subdir);

char *get_directory(priority_t which);

int check_directory(const char *path) /*@globals errno,stderr@*/;

#endif	/* PATHS_H */
