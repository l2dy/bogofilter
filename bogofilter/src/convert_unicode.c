/* $Id$ */

/*****************************************************************************

NAME:
   charset_iconv.c -- provide charset support using iconv().

Note:

   Character translation is done to make life easier for the lexer.
   Text is changed only after the message has been saved for
   passthrough.  The end user (mail reader) never sees any changes -
   only the lexer.

AUTHOR:
   David Relson <relson@osagesoftware.com>  2005

******************************************************************************/

/*****************************************************************************
 **
 **    EXPERIMENTAL    EXPERIMENTAL    EXPERIMENTAL    EXPERIMENTAL
 **
 ** This is test code -- mostly a proof of concept; not ready for prime time.
 **
******************************************************************************/

#include "common.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "charset.h"
#include "convert_unicode.h"
#include "chUnicodeTo866.h"
#include "xmalloc.h"
#include "xstrdup.h"

#define	SP	' '

#include <iconv.h>
iconv_t cd;

static void map_nonascii_characters(void);

static void map_nonascii_characters(void)
{
    uint ch;
    for (ch = 0; ch < COUNTOF(charset_table); ch += 1)
    {
	/* convert high-bit characters to '?' */
	if (ch & 0x80 && casefold_table[ch] == ch)
	    casefold_table[ch] = '?';
    }
}

static void map_default(void)
{
    unsigned int ch;

    for (ch = 0; ch < COUNTOF(charset_table); ch += 1)
    {
	charset_table[ch] = casefold_table[ch] = ch;
    }

    for (ch=0; ch < COUNTOF(charset_table); ch += 1)
    {
	if (iscntrl(ch) &&		/* convert control characters to blanks */
	    ch != '\t' && ch != '\n')	/* except tabs and newlines		*/
	    charset_table[ch] = SP;
    }
}

typedef struct charset_def {
    const char *name;
    bool allow_nonascii_replacement;
} charset_def_t;

#define	T	true
#define	F	false

static charset_def_t charsets[] = {
    { "default",	T },
    { "us-ascii",	T },
    { "utf-8",		T },
    { "iso8859-1",	T },		/* ISOIEC 8859-1:1998 Latin Alphabet No. 1	*/
    /* tests/t.systest.d/inputs/spam.mbx is iso-8859-1 and contains 8-bit characters - " “Your Account” " */
    { "iso8859-2",	F },		/* ISOIEC 8859-2:1999 Latin Alphabet No. 2	*/
    { "iso8859-3",	F },		/* ISOIEC 8859-3:1999 Latin Alphabet No. 3	*/
    { "iso8859-4",	F },		/* ISOIEC 8859-4:1998 Latin Alphabet No. 4	*/
    { "iso8859-5",	F },		/* ISOIEC 8859-5:1999 LatinCyrillic Alphabet	*/
    { "iso8859-6",	F },		/* ISOIEC 8859-6:1999 LatinArabic Alphabet	*/
    { "iso8859-7",	F },		/* ISO	  8859-7:1987 LatinGreek Alphabet	*/
    { "iso8859-8",	F },		/* ISOIEC 8859-8:1999 LatinHebrew Alphabet	*/
    { "iso8859-9",	F },		/* ISOIEC 8859-9:1999 Latin Alphabet No. 5	*/
    { "iso8859-10",	F },		/* ISOIEC 8859-10:1998 Latin Alphabet No. 6	*/
    { "iso8859-13",	F },		/* ISOIEC 8859-13:1998 Latin Alphabet No. 7 (Baltic Rim)*/
    { "iso8859-14",	F },		/* ISOIEC 8859-14:1998 Latin Alphabet No. 8 (Celtic)	*/
    { "iso8859-15",	F },		/* ISOIEC 8859-15:1999 Latin Alphabet No. 9		*/
    { "cp866",		F },
    { "koi8-r",		F },
    { "windows-1251",	F },
    { "windows-1252",	T },
    { "windows-1256",	T },
    { "iso2022-jp",	T },		/* rfc-1468 - japanese */
    { "euc-kr",		T },		/* extended unix code for korean */
    { "iso2022-kr",	T },		/* korean standard code (7-bit)*/
    { "ks-c-5601-1987",	T },		/* korean standard (default) */
    { "big5",		T },
    { "csbig5",		T },
    { "gb2312",		T },
    { "csgb2312",	T },
};

void init_charset_table_iconv(const char *charset_name)
{
    uint idx;

    if (cd != NULL)
	iconv_close(cd);

/*  iconv_t iconv_open(const char *tocode, const char *fromcode); */
    if (DEBUG_ICONV(1))
	fprintf(dbgout, "converting %s to %s\n", charset_default, charset_name);
    cd = iconv_open( charset_default, charset_name );
    if (cd == (iconv_t)(-1)) {
	int err = errno;
	if (err != EINVAL)
	    fprintf( stderr, "Invalid charset '%s'\n", charset_name );
	cd = iconv_open( charset_default, charset_default );
    }

    for (idx = 0; idx < COUNTOF(charsets); idx += 1)
    {
	charset_def_t *charset = &charsets[idx];
	if (strcasecmp(charset->name, charset_name) == 0)
	{
	    map_default();	/* Setup the table defaults. */
	    if (replace_nonascii_characters)
		if (charset->allow_nonascii_replacement)
		    map_nonascii_characters();
	    break;
	}
    }

    return;
}
