/* $Id$ */

/**
 * \file
 * paths.c -- routines for working with file paths.
 */

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

static pri_2_env_t pri_2_env[] = {
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
	snprintf(type, len, "%s-%s", name, db_type);
    }
    return type;
}

char *build_path(const char* path, const char* file)
{
    size_t pathlen = strlen(path);
    size_t filelen = strlen(file);
    size_t size = pathlen + filelen + strlen(DIRSEP_S) + 1;
    char  *dest = xmalloc( size );
    
    /* If absolute path ... */
    if (bf_abspath(file))
    {
	memcpy(dest, file, filelen+1);
	return dest;
    }

    memcpy(dest, path, pathlen+1);

    if (pathlen >= filelen && strcmp(path+(pathlen-filelen), file) == 0)
	return dest;

    if (!is_file(path) && check_directory(path)) {
	if (dest[strlen(dest)-1] != DIRSEP_C)
	    strlcat(dest, DIRSEP_S, size);
	strlcat(dest, file, size);
    }

    return dest;
}

char *create_path_from_env(const char *var,
		/*@null@*/ const char *subdir)
{
    char *buff, *env;
    size_t path_size, env_size;

    env = getenv(var);
    if (env == NULL || *env == '\0') return NULL;

    env_size = strlen(env);
    path_size = env_size + (subdir ? strlen(subdir) : 0) + 2;
    buff = xmalloc(path_size);

    strlcpy(buff, env, path_size);
    if (subdir != NULL) {
	if (buff[env_size-1] != DIRSEP_C)
	    strlcat(buff, DIRSEP_S, path_size);
	strlcat(buff, subdir, path_size);
    }
    if (strlcat(buff, "", path_size) >= path_size)
	abort(); /* buffer overrun, this cannot happen - buff is xmalloc()d */
    return buff;
}

bool check_directory(const char* path) /*@globals errno,stderr@*/
{
    int rc;
    struct stat sb;

    if (path == NULL || *path == '\0')
	return false;

    rc = stat(path, &sb);
    if (rc < 0) {
	if (ENOENT==errno) {
	    if (bf_mkdir(path, S_IRUSR|S_IWUSR|S_IXUSR)) {
		fprintf(stderr, "Error creating directory '%s': %s\n",
			path, strerror(errno));
		return false;
	    } else if (verbose > 0) {
		fprintf(dbgout, "Created directory %s .\n", path);
	    }
	    return true;
	} else {
	    fprintf(stderr, "Error accessing directory '%s': %s\n",
		    path, strerror(errno));
	    return false;
	}
    } else {
	if (! S_ISDIR(sb.st_mode)) {
	    fprintf(stderr, "Error: %s is not a directory.\n", path);
	    return false;
	}
    }
    return true;
}

bool is_file(const char* path) /*@globals errno,stderr@*/
{
    int rc;
    struct stat sb;

    if (path == NULL || *path == '\0')
	return false;

    rc = stat(path, &sb);

    if (rc == 0 && !S_ISDIR(sb.st_mode))
	return true;
    else
	return false;
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

/** returns malloc()ed copy of the file name part of \a path.
 */
char *get_file_from_path(const char *path)
{
    char *file = strrchr(path, DIRSEP_C);
    if (file == NULL)
	file = xstrdup(path);
    else
	file = xstrdup(file + 1);
    return file;
}

/** returns malloc()ed copy of the directory name of \a path.
 */
char *get_directory_from_path(const char *path)
{
    char *dir = xstrdup(path);
    char *last = strrchr(dir, DIRSEP_C);
    if (last == NULL) {
	xfree(dir);
	return NULL;
    }
    else {
	*last = '\0';
	return dir;
    }
}
