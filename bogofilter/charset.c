/* $Id$ */

/*****************************************************************************

NAME:
   charset.c -- provide charset support for bogofilter's lexer.

Note:

   Character translation is done to make life easier for the lexer.
   Text is changed only after the message has been saved for
   passthrough.  The end user (mail reader) never sees any changes -
   only the lexer.

AUTHOR:
   David Relson <relson@osagesoftware.com>

******************************************************************************/

/* imports */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "common.h"

#include "charset.h"
#include "lexer.h"
#include "xmalloc.h"
#include "xstrdup.h"

#define	SP	' '

const char *charset_default = "us-ascii";
bool replace_nonascii_characters = false;

unsigned char charset_table[256];
unsigned char casefold_table[256];

static void map_default(void);
static void map_us_ascii(void);

static void map_iso_8859_1(void);
static void map_iso_8859_2(void);
static void map_iso_8859_3(void);
static void map_iso_8859_4(void);
static void map_iso_8859_5(void);
static void map_iso_8859_6(void);
static void map_iso_8859_7(void);
static void map_iso_8859_8(void);
static void map_iso_8859_9(void);
static void map_iso_8859_10(void);
static void map_iso_8859_13(void);
static void map_iso_8859_14(void);
static void map_iso_8859_15(void);

static void map_unicode(void);

static void map_windows_1251(void);
static void map_windows_1252(void);
static void map_windows_1256(void);

static void map_nonascii_characters(void);

#define	DEBUG
#undef	DEBUG

#ifndef	DEBUG
#define	PRINT_CHARSET_TABLE
#else
#define	PRINT_CHARSET_TABLE	print_charset_table()
#undef	DEBUG_GENERAL
#define	DEBUG_GENERAL(level)	(verbose >= level)
#endif

#ifdef	DEBUG
static void print_charset_table(void)
{
    int c,r,i;
    int ch;
    if (!DEBUG_GENERAL(1))
	return;
    printf( "\n" );
    for (r=0; r<4; r+=1) {
	for (c=0; c<64; c+=1) {
	    i=r*64+c;
	    ch=charset_table[i];
	    if (ch != 0x08 && ch != 0x09 && ch != '\n' && ch != '\r')
		printf(" %02X.%2c.%02X", i, ch, ch);
	    else
		printf(" %02X.%02X.%02X", i, ch, ch);
	    if ((c & 15) == 15)
		printf( "\n" );
	}
    }
    printf( "\n" );
}
#endif

static void map_nonascii_characters(void)
{
    size_t ch;
    for (ch = 0; ch < COUNTOF(charset_table); ch += 1)
    {
	/* convert uppercase to lower case */
	if (isupper(ch))
	    casefold_table[ch] = tolower(ch);
	/* convert high-bit characters to '?' */
	if (ch & 0x80 && casefold_table[ch] == ch)
	    casefold_table[ch] = '?';
    }
}

static void map_xlate_characters( unsigned char *xlate, size_t size )
{
    size_t i;
    for (i = 0; i < size; i += 2)
    {
	unsigned char from = xlate[i];
	unsigned char to   = xlate[i+1];
	charset_table[from] = to;
    }
}

static void map_default(void)
{
    unsigned int ch;

    const char *alone = "\n\t";				/* Characters to leave alone */
    const char *blank = "!$%&()*+/;<>?[\\]^`{|}~";	/* Characters to treat as blanks */

    for (ch = 0; ch < COUNTOF(charset_table); ch += 1)
    {
	charset_table[ch] = casefold_table[ch] = ch;
	casefold_table[ch] = isupper(ch) ? ch + 'a' - 'A' : ch;
    }

    PRINT_CHARSET_TABLE;

    for (ch=0; ch < COUNTOF(charset_table); ch += 1)
    {
	if (iscntrl(ch))				/* convert control characters to blanks */
	{
	    charset_table[ch] = SP;
	    continue;
	}
    }

    /* Characters to treat as delimiters.  The ideal would be to treat them as blanks, but
       that breaks ignoring uuencode, so treat them as '!' (another char not allowed in tokens)
    */
    while ((ch = *blank++) != '\0')
	charset_table[ch] = '!';

    while ((ch = *alone++) != '\0')			/* Characters to leave alone */
	charset_table[ch] = ch;

    PRINT_CHARSET_TABLE;
}

static void map_iso_8859_1(void)	/* ISOIEC 8859-1:1998 Latin Alphabet No. 1 */
{
    /* Not yet implemented */
}


static void map_iso_8859_2(void)	/* ISOIEC 8859-2:1999 Latin Alphabet No. 2 */
{
    /* Not yet implemented */
}

static void map_iso_8859_3(void)	/* ISOIEC 8859-3:1999 Latin Alphabet No. 3 */
{
    /* Not yet implemented */
}

static void map_iso_8859_4(void)	/* ISOIEC 8859-4:1998 Latin Alphabet No. 4 */
{
    /* Not yet implemented */
}

static void map_iso_8859_5(void)	/* ISOIEC 8859-5:1999 LatinCyrillic Alphabet */
{
    /* Not yet implemented */
}

static void map_iso_8859_6(void)	/* ISOIEC 8859-6:1999 LatinArabic Alphabet */
{
    /* Not yet implemented */
}

static void map_iso_8859_7(void)	/* ISO 8859-7:1987 LatinGreek Alphabet     */
{
    /* Not yet implemented */
}

static void map_iso_8859_8(void)	/* ISOIEC 8859-8:1999 LatinHebrew Alphabet */
{
    /* Not yet implemented */
}

static void map_iso_8859_9(void)	/* ISOIEC 8859-9:1999 Latin Alphabet No. 5 */
{
    /* Not yet implemented */
}

static void map_iso_8859_10(void)	/* ISOIEC 8859-10:1998 Latin Alphabet No. 6 */
{
    /* Not yet implemented */
}

static void map_iso_8859_13(void)	/* ISOIEC 8859-13:1998 Latin Alphabet No. 7 (Baltic Rim) */
{
    /* Not yet implemented */
}

static void map_iso_8859_14(void)	/* ISOIEC 8859-14:1998 Latin Alphabet No. 8 (Celtic) */
{
    /* Not yet implemented */
}

static void map_iso_8859_15(void)	/* ISOIEC 8859-15:1999 Latin Alphabet No. 9 */
{
    static unsigned char xlate_15[] = {
	'\xA0', ' ',		/* A0  160      160 NO-BREAK SPACE */
	'\xA1', '!',		/* A1  161  ¡   161 INVERTED EXCLAMATION MARK */
	'\xA2', '$',		/* A2  162  ¢   162 CENT SIGN */
	'\xA3', '$',		/* A3  163  £   163 POUND SIGN */
	'\xA4', '$',		/* A4  164 EUR 8364 EURO SIGN */
	'\xA5', '$',		/* A5  165  ¥   165 YEN SIGN */

	'\xA7', ' ',		/* A7  167  §   167 SECTION SIGN */
	'\xA9', ' ',		/* A9  169  ©   169 COPYRIGHT SIGN */
	'\xAA', ' ',		/* AA  170  ª   170 FEMININE ORDINAL INDICATOR */
	'\xAB', '\"',		/* AB  171  «   171 LEFT-POINTING DOUBLE ANGLE QUOTATION MARK */
	'\xAC', ' ',		/* AC  172  ¬   172 NOT SIGN */
	'\xAD', ' ',		/* AD  173      173 SOFT HYPHEN */
	'\xAE', ' ',		/* AE  174  ®   174 REGISTERED SIGN */
	'\xAF', ' ',		/* AF  175  ¯   175 MACRON */
	'\xB0', ' ',		/* B0  176  °   176 DEGREE SIGN */
	'\xB1', ' ',		/* B1  177  ±   177 PLUS-MINUS SIGN */
	'\xB2', ' ',		/* B2  178  ²   178 SUPERSCRIPT TWO */
	'\xB3', ' ',		/* B3  179  ³   179 SUPERSCRIPT THREE */
	'\xB5', ' ',		/* B5  181  µ   181 MICRO SIGN */
	'\xB6', ' ',		/* B6  182  ¶   182 PILCROW SIGN */
	'\xB7', ' ',		/* B7  183  ·   183 MIDDLE DOT */
	'\xB9', ' ',		/* B9  185  ¹   185 SUPERSCRIPT ONE */
	'\xBA', ' ',		/* BA  186  º   186 MASCULINE ORDINAL INDICATOR */
	'\xBB', '\"',		/* BB  187  »   187 RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK */
	'\xBC', ' ',		/* BC  188 OE   338 LATIN CAPITAL LIGATURE OE */
	'\xBD', ' ',		/* BD  189 oe   339 LATIN SMALL LIGATURE OE */
	'\xBF', '?',		/* BF  191  ¿   191 INVERTED QUESTION MARK */
	'\xD7', '*',		/* D7  215  ×   215 MULTIPLICATION SIGN */
	'\xF7', '/',		/* F7  247  ÷   247 DIVISION SIGN */
    };
    map_xlate_characters( xlate_15, COUNTOF(xlate_15) );
    /* Not yet implemented */
}

static unsigned char xlate_us[] = {
    '\xA0', ' ',	/* no-break space      to space        */
    '\x92', '\'',	/* windows apostrophe  to single quote */
    '\x93', '"',	/* windows left  quote to double quote */
    '\x94', '"',	/* windows right quote to double quote */
    '\xA9', ' '		/* copyright sign      to space        */
};

/* For us-ascii, 
** first do the iso-8859-1 setup
** then addin special characters.
*/

static void map_us_ascii(void)
{
    map_iso_8859_1();

    map_xlate_characters( xlate_us, COUNTOF(xlate_us) );
}
static void map_unicode(void)
{
    /* Not yet implemented */
}

static void map_windows_1251(void)
{
    /* Not yet implemented */
}

static void map_windows_1252(void)
{
    /* Not yet implemented */
}

static void map_windows_1256(void)
{
    /* Not yet implemented */
}

static void map_chinese(void)
{
    /* Not yet implemented */
}

static void map_korean(void)
{
    /* Not yet implemented */
}

static void map_japanese(void)
{
    /* Not yet implemented */
}

typedef void (*charset_fcn) (void);

typedef struct charset_def {
    const char *name;
    charset_fcn func;
    bool allow_nonascii_replacement;
} charset_def_t;

#define	T	true
#define	F	false

charset_def_t charsets[] = {
    { "default",	&map_default,	   T },
    { "us-ascii",	&map_us_ascii,	   T },
#if	1
    { "iso8859-1",	&map_us_ascii,	   T },		/* ISOIEC 8859-1:1998 Latin Alphabet No. 1	*/
    /* tests/t.systest.d/inputs/spam.mbx is iso-8859-1 and contains 8-bit characters - " “Your Account” " */
#else
    { "iso8859-1",	&map_iso_8859_1,   T },		/* ISOIEC 8859-1:1998 Latin Alphabet No. 1	*/
#endif
    { "iso8859-2",	&map_iso_8859_2,   F },		/* ISOIEC 8859-2:1999 Latin Alphabet No. 2	*/
    { "iso8859-3",	&map_iso_8859_3,   F },		/* ISOIEC 8859-3:1999 Latin Alphabet No. 3	*/
    { "iso8859-4",	&map_iso_8859_4,   F },		/* ISOIEC 8859-4:1998 Latin Alphabet No. 4	*/
    { "iso8859-5",	&map_iso_8859_5,   F },		/* ISOIEC 8859-5:1999 LatinCyrillic Alphabet	*/
    { "iso8859-6",	&map_iso_8859_6,   F },		/* ISOIEC 8859-6:1999 LatinArabic Alphabet	*/
    { "iso8859-7",	&map_iso_8859_7,   F },		/* ISO 8859-7:1987 LatinGreek Alphabet		*/
    { "iso8859-8",	&map_iso_8859_8,   F },		/* ISOIEC 8859-8:1999 LatinHebrew Alphabet	*/
    { "iso8859-9",	&map_iso_8859_9,   F },		/* ISOIEC 8859-9:1999 Latin Alphabet No. 5	*/
    { "iso8859-10",	&map_iso_8859_10,  F },		/* ISOIEC 8859-10:1998 Latin Alphabet No. 6	*/
    { "iso8859-13",	&map_iso_8859_13,  F },		/* ISOIEC 8859-13:1998 Latin Alphabet No. 7 (Baltic Rim)*/
    { "iso8859-14",	&map_iso_8859_14,  F },		/* ISOIEC 8859-14:1998 Latin Alphabet No. 8 (Celtic)	*/
    { "iso8859-15",	&map_iso_8859_15,  F },		/* ISOIEC 8859-15:1999 Latin Alphabet No. 9		*/
    { "windows-1251",	&map_windows_1251, T },
    { "windows-1252",	&map_windows_1252, T },
    { "windows-1256",	&map_windows_1256, T },
    { "utf-8",		&map_unicode,	   T },
    { "iso2022-jp",	&map_japanese,	   T },	/* rfc-1468 - japanese */
    { "euc-kr",		&map_korean,	   T },	/* extended unix code for korean */
    { "iso2022-kr",	&map_korean,	   T },	/* korean standard code (7-bit)*/
    { "ks-c-5601-1987",	&map_korean,	   T },	/* korean standard (default) */
    { "big5",		&map_chinese,	   T },
    { "csbig5",		&map_chinese,	   T },
    { "gb2312",		&map_chinese,	   T },
    { "csgb2312",	&map_chinese,	   T },
};

#define	MAX_LEN 64

void init_charset_table(const char *charset_name, bool use_default)
{
    size_t idx;
    bool found = false;
    for (idx = 0; idx < COUNTOF(charsets); idx += 1)
    {
	charset_def_t *charset = &charsets[idx];
	if (strcasecmp(charset->name, charset_name) == 0)
	{
	    if (DEBUG_CONFIG(1))
		printf(" ... found.\n");
	    map_default();			/* Setup the table defaults. */
	    (*charset->func)();
	    found = true;
	    if (replace_nonascii_characters &&
		charset->allow_nonascii_replacement)
		map_nonascii_characters();
	    break;
	}
    }

    if ( !found && use_default ) {
	map_default();
	if ( !quiet && verbose > 0)
	    fprintf( stderr, "Can't find charset '%s';  using default.\n", charset_name );
    }

    return;
}


void got_charset( const char *charset )
{
    char *lcl;
    char *tmp = strchr( charset, '=' );
    char *s, *d;
    lcl = xstrdup( tmp ? tmp+1 : charset);
    for (s = d = lcl; *s != '\0'; s++)
    {
	if (*s == '"')		/* skip quotes */
	    continue;
	if (*s == '_')		/* map underscore to dash */
	    *s = '-';
	*d++ = tolower(*s);	/* map upper case to lower */
	if (*s == '-' &&	/* map "iso-" to "iso"     */
	    memcmp(lcl, "iso-", 4) == 0)
	    d -= 1;
    }
    *d++ = '\0';
    if (DEBUG_CONFIG(0))
	fprintf( stderr, "got_charset( '%s' )\n", lcl );
    init_charset_table( lcl, false );
    xfree(lcl);
}
