/* find_home_tildeexpand.c -- library function to do sh-like ~ expansion */

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

#include <string.h>

#include "xmalloc.h"
#include "xstrdup.h"
#include "find_home.h"
#include "common.h"

/*
 * tildeexpand tries to expand the first tilde argument, similar, but
 * not identical, to the way a POSIX sh does. It does not support
 * multiple tildes, and does not search for a slash, but for the longest
 * count of characters in the POSIX portable file name character set.
 */
/*@only@*/
char *tildeexpand(const char *name) {
    char *tmp;
    const char *home;
    size_t l;

    if (name[0] != '~')
	return xstrdup(name);

    l = strspn(&name[1], 
	    "abcdefghijklmnopqrstuvwxyz"
	    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	    "0123456789._-"); /* Portable Filename Character Set */
    if (l) {
	/* got a parameter to the tilde */
	tmp = xmalloc(l + 1);
	strncpy(tmp, &name[1], l);
	tmp[l] = '\0';

	home = find_home_user(tmp);
    } else {
	/* plain tilde */
	home = find_home(FALSE);
	if (home == NULL)
	    return xstrdup(name);
	tmp = NULL;
    }

    if (home == NULL) {
	xfree(tmp);
	return xstrdup(name);
    }

    xfree(tmp);
    tmp = xmalloc(strlen(name) - l + strlen(home));
    strcpy(tmp, home);
    /* no need to insert a slash here, name[l] should contain one */
    strcat(tmp, name + l + 1);
    return tmp;
}
