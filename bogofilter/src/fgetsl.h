/* $Id$ */

/*****************************************************************************

NAME:
   fgetsl.h -- prototype for fgetsl.c

******************************************************************************/

#ifndef FGETSL_H
#define FGETSL_H

#include <stdio.h>
#include "buff.h"

extern int fgetsl(buff_t *buff, FILE *fp);
extern int xfgetsl(buff_t *buff, FILE *fp, int no_NUL_terminate);

#endif
