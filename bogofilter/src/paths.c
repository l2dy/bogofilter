/* $Id$ */

/*****************************************************************************

NAME:
   paths.c -- routines for working with file paths.

******************************************************************************/

#include "common.h"

#include <errno.h>
#include <sys/stat.h>
#include <stdlib.h>

#include "paths.h"
#include "xmalloc.h"
#include "xstrdup.h"

/* Local  Data */

typedef struct {
    priority_t p;	/* precedence */
    const char *v;	/* env var    */
    const char *s;	/* sub dir    */
} pri_2_env_t;

pri_2_env_t pri_2_env[] = {
#ifndef __riscos__
    { PR_ENV_BOGO, "BOGOFILTER_DIR", NULL },
    { PR_ENV_BOGO, "BOGODIR",	     NULL },
    { PR_ENV_HOME, "HOME",	     BOGODIR }
#else
    { PR_ENV_HOME, "Choices$Write",  BOGODIR },
    { PR_ENV_HOME, "Bogofilter$Dir", NULL },
#endif
};

/* Function Definitions */

char *build_progtype(const char *name, const char *db_type) 
{
    char *type;
    if (strcmp(db_type, "db") == 0)
	type = xstrdup(name);
    else {
	size_t len = strlen(name) + strlen(db_type) + 2;
	type = xmalloc(len);
	sprintf(type, "%s-%s", name, db_type);
    }
    return type;
}

/* build an path to a file given a directory and file name,
 * concatenating dir and file, adding a slash if necessary
 *
 * returns: -1 for overflow
 *	     0 for success
 */
int build_path(char* dest, size_t size, const char* dir, const char* file)
{
    /* If absolute path ... */
    if (bf_abspath(file))
    {
	if (strlcpy(dest, file, size) >= size) 
	    return -1;
	else
	    return 0;
    }

    if (strlcpy(dest, dir, size) >= size) return -1;

    if (dest[strlen(dest)-1] != DIRSEP_C) {
	if (strlcat(dest, DIRSEP_S, size) >= size) return -1; /* RATS: ignore */
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
    if (subdir != NULL) {
	if (buff[env_size-1] != DIRSEP_C)
	    strcat(buff, DIRSEP_S);
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
	    if (bf_mkdir(path, S_IRUSR|S_IWUSR|S_IXUSR)) {
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
    size_t i;
    char *dir = NULL;

    for (i = 0; i < COUNTOF(pri_2_env) ; i += 1) {
	pri_2_env_t *p2e = &pri_2_env[i];
	if (p2e->p == which) {
	    dir = create_path_from_env(p2e->v, p2e->s);
	    if (dir)
		break;
	}
    }
    return dir;
}
