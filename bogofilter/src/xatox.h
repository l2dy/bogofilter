/* $Id$ */

/** \file xatox.h
 * Header file for xato* string-to-number conversion
 * functions with simplified error checking.
 *
 * \author Matthias Andree
 * \date 2003
 */

#ifndef XATOX_H
#define XATOX_H

/** atoi variant with error checking. The function tries to parse
 * the integer string \a in into \a d using the \b strtol(3) ANSI C function.
 * \return
 * - 0 - failure, out of range or illegal characters
 * - 1 - success
 */
int xatoi(int *d /** the result is stored here */ /*@out@*/,
	  const char *in /** input string */);

/** strtod variant with simplified error checking. The function tries to parse
 * the floating point string \a in into \a d using the \b strtod(3) ANSI
 * C function.  \return
 * - 0 - failure, out of range or illegal characters
 * - 1 - success
 */
int xatof(double *d /** the result is stored here */ /*@out@*/, 
	  const	char *in /** input string */);

#endif
