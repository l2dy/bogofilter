/* $Id$ */

/*****************************************************************************

NAME:
   fgetsl.h -- prototype for fgetsl.c

******************************************************************************/

#ifndef FGETSL_H
#define FGETSL_H

#include <stdio.h>

extern int fgetsl(char *, int, FILE *);
extern int xfgetsl(char *, int, FILE *i, int no_NUL_terminate);

#endif
