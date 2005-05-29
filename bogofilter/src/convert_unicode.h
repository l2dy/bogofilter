/* $Id$ */

/*****************************************************************************

NAME:
   charset_unicode.h -- constants and declarations for charset_unicode.c

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#ifndef	CHARSET_UNICODE_H
#define	CHARSET_UNICODE_H

extern void init_charset_table_iconv(const char *charset_name);

#if	defined(CP866) && !defined(ENABLE_UNICODE) && !defined(DISABLE_UNICODE)
extern int  decode_and_htmlUNICODE_to_cp866(byte *buf, int len);
#endif

#endif /* CHARSET_UNICODE_H */
