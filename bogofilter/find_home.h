/* find_home.h -- library function to figure out the home dir of current user */

#ifndef FIND_HOME_H
#define FIND_HOME_H

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

/*@null@*/ /*@observer@*/
const char *
find_home(int read_env) /*@globals errno@*/;

#endif
