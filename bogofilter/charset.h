/* $Id$ */

/*****************************************************************************

NAME:
   charset.h -- constants and declarations for charset.c

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#ifndef	HAVE_CHARSET_H
#define	HAVE_CHARSET_H

extern const char *charset_default;
extern bool replace_nonascii_characters;

extern unsigned char charset_table[256];
extern unsigned char casefold_table[256];
extern void got_charset(const char *);
extern void init_charset_table(const char *charset_name, bool use_default);

#endif	/* HAVE_CHARSET_H */
