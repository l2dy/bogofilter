/* $Id$ */

/*****************************************************************************

NAME:
   bogohome.c -- support for bogofilter's home directory.

AUTHOR:
   (C) Copyright 2004
   Matthias Andree <matthias.andree@gmx.de>

LICENSE:
   GNU General Public License V2

******************************************************************************/

#include "config.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>

#include "bogohome.h"
#include "find_home.h"
#include "xmalloc.h"
#include "xstrdup.h"

char *bogohome=NULL;

void set_bogohome(const char *path) {
    char *t;
    struct stat st;

    if (bogohome)
	xfree(bogohome);

    bogohome = tildeexpand(path);
    if (stat(bogohome, &st) != 0 || !S_ISDIR(st.st_mode)) {
	if ((t = strrchr(bogohome, DIRSEP_C)))
	    *t = '\0';
	else
	    *bogohome = '\0';
    }
    if (*bogohome == '\0') {
	xfree(bogohome);
	bogohome = xstrdup(CURDIR_S);
    }
}


void free_bogohome(void)
{
    xfree(bogohome);
    bogohome = NULL;
}
