/* $Id$ */

/*****************************************************************************

NAME:
   bogoreader.h -- prototypes and definitions for bogoreader.c.

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#ifndef BOGOREADER_H
#define BOGOREADER_H

#include "buff.h"

/* Function Prototypes */

extern void bogoreader_init(int argc, char **argv);
extern void bogoreader_fini(void);
void bogoreader_name(const char *name);

/* Lexer-Reader Interface */

typedef int   reader_line_t(buff_t *buff);
typedef bool  reader_more_t(void);
typedef const char *reader_file_t(void);

extern reader_line_t *reader_getline;
extern reader_more_t *reader_more;
extern reader_file_t *reader_filename;

#endif
