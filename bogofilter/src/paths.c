/* $Id$ */

/*****************************************************************************

NAME:
   paths.c -- routines for working with file paths.

******************************************************************************/

#include <errno.h>
#include <sys/stat.h>
#include <stdlib.h>

#include <config.h>
#include "common.h"

#include "paths.h"
#include "xmalloc.h"

#define BOGODIR ".bogofilter"

/* build an path to a file given a directory and file name,
 * concatenating dir and file, adding a slash if necessary
 *
 * returns: -1 for overflow
 *	     0 for success
 */
int build_path(char* dest, size_t size, const char* dir, const char* file)
{
    if (strlcpy(dest, dir, size) >= size) return -1;

    if ('/' != dest[strlen(dest)-1]) {
	if (strlcat(dest, "/", size) >= size) return -1; /* RATS: ignore */
    }

    if (strlcat(dest, file, size) >= size) return -1;

    return 0;
}

/* if the given environment variable 'var' exists, create a path from it and
   tack on the optional 'subdir' value.
   return value: buffer address if successful
                 NULL if failure
 */
char *create_path_from_env(const char *var,
		/*@null@*/ const char *subdir)
{
    char *buff, *env;
    size_t path_size, env_size;

    env = getenv(var);
    if (env == NULL || strlen(env) == 0) return NULL;

    env_size = strlen(env);
    path_size = env_size + (subdir ? strlen(subdir) : 0) + 2;
    buff = xmalloc(path_size);

    strcpy(buff, env);
    if ('/' != buff[env_size-1]) {
	strcat(buff, "/");
    }
    if (subdir != NULL) {
	strcat(buff, subdir);
    }
    return buff;
}

/* check that our directory exists and try to create it if it doesn't
   return -1 on failure, 0 otherwise.
 */
int check_directory(const char* path) /*@globals errno,stderr@*/
{
    int rc;
    struct stat sb;

    if (path == NULL || *path == '\0') {
	return -1;
    }

    rc = stat(path, &sb);
    if (rc < 0) {
	if (ENOENT==errno) {
	    if(mkdir(path, S_IRUSR|S_IWUSR|S_IXUSR)) {
		fprintf(stderr, "Error creating directory \"%s\": %s\n",
			path, strerror(errno));
		return -1;
	    } else if (verbose > 0) {
		(void)fprintf(dbgout, "Created directory %s .\n", path);
	    }
	    return 0;
	} else {
	    fprintf(stderr, "Error accessing directory \"%s\": %s\n",
		    path, strerror(errno));
	    return -1;
	}
    } else {
	if (! S_ISDIR(sb.st_mode)) {
	    (void)fprintf(stderr, "Error: %s is not a directory.\n", path);
	}
    }
    return 0;
}

char *get_directory(priority_t which)
{
    char *dir = NULL;

    if (which == PR_ENV_BOGO) {
	dir = create_path_from_env("BOGOFILTER_DIR", NULL);
	if (dir == NULL)
	    dir = create_path_from_env("BOGODIR", NULL);
    }

    if (which == PR_ENV_HOME) {
	dir = create_path_from_env("HOME", BOGODIR);
    }

    return dir;
}

