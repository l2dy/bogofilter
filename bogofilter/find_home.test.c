/* find_home.test.c -- program to test find_home library */

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

#include <stdlib.h>
#include <stdio.h>

#include "find_home.h"

int main(int argc, char **argv)
{
    const char *h;
    int read_env = 0;

    if (argc >= 2)
	read_env = atoi(argv[1]);

    h = find_home(read_env);
    if (h != NULL) {
	(void)puts(h);
	exit(EXIT_SUCCESS);
    } else {
	exit(EXIT_FAILURE);
    }
}
