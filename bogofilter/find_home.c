/* find_home.c -- library function to figure out the home dir of current user */

/* (C) 2002 by Matthias Andree <matthias.andree@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details, it is in the file named
 * COPYING.
 */

/* $Id$ */

#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>

#include "find_home.h"
#include "string.h"
#include "xmalloc.h"
#include "xstrdup.h"

/* This function will try to figure out the home directory of the user.
 * 
 * If read_env is not zero, it will try to obtain the HOME environment
 * variable and return if it is defined and not empty.
 * 
 * Then, it will look up the password entry of the current effective
 * user id and return the pw_dir field.
 *
 * This function returns NULL in case of failure.
 */
const char *find_home(int read_env) {
    char *r;
    uid_t u;
    struct passwd *pw;

    if (read_env) {
	if ((r = getenv("HOME")) != NULL && *r != '\0')
	return r;
    }

    u = geteuid();
    pw = getpwuid(u);
    if (pw != NULL) {
	return pw -> pw_dir;
    }
    return NULL;
}

char *resolve_home_directory(const char *filename)
{
    char *tmp;
    const char *home = find_home(1);
    if (home == NULL)
    {
	fprintf(stderr, "Can't find home directory.\n");
	exit(2);
    }
    tmp = xmalloc(strlen(home) + strlen(filename) + 2);
    strcpy(tmp, home);
    if (tmp[strlen(tmp)-1] != '/')
	strcat(tmp, "/");
    strcat(tmp, filename);
    return tmp;
}
