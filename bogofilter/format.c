/* $Id$ */

/*****************************************************************************

NAME:
   format.c -- formats a *printf-like string for ouput

Most of the ideas in here are stolen from Mutt's snprintf implementation.

******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>

#include <config.h>
#include "common.h"

#include "system.h"

#include "format.h"
#include "bogoconfig.h"
#include "bogofilter.h"
#include "method.h"
#include "xmalloc.h"
#include "xstrdup.h"

/* Function Prototypes */
static int  format_float( char *dest, double val,      int min, int prec, int flags, const char *destend);
static int  format_string(char *dest, const char *val, int min, int prec, int flags, const char *destend);
static int  format_spamicity(char *dest, const char *fmt, double spamicity, const char *destend);
static void die (const char *msg, ...);
static char *convert_format_to_string(char *buff, size_t size, const char *format);

/* uninitialized static variables */

char reg = ' ';
int wrdcount = 0;
int msgcount = 0;

#define cls_cnt 3			/* 3 (for Spam/Ham/Unsure) */
typedef const char *pchar;
pchar spamicity_tags[cls_cnt]    = {  "Yes",   "No",   "Unsure" };
pchar spamicity_formats[cls_cnt] = { "%0.6f", "%0.6f", "%0.6f"  };

/* initialized static variables */

const char *spam_header_name = SPAM_HEADER_NAME;	/* used by lexer */

/*
**	formatting characters:
**
**	    h - spam_header_name, e.g. "X-Bogosity"
**
**	    a - algorithm, e.g. "graham", "robinson", "fisher"
**
**	    c - classification, e.g. Yes/No, Spam/Ham/Unsure, +/-/?
**
**	    e - spamicity as 'e' format
**	    f - spamicity as 'f' format
**
**	    l - logging tag (from '-l' option)
**
**	    o - spam_cutoff, ex. cutoff=%c
**
**	    r - runtype
**	        w - word count
**	        m - message count
**
**	    v - version, ex. "version=%v"
*/

static const char *header_format = "%h: %c, tests=bogofilter, spamicity=%p, version=%v";
static const char *terse_format = "%1.1c %f";
static const char *log_header_format = "%h: %c, spamicity=%p, version=%v";
static const char *log_update_format = "register-%r, %w words, %m messages";

static bool set_spamicity_tags(const char *val);
static bool set_spamicity_formats(const char *val);
static bool set_spamicity_fields(pchar *strings, const char *val);

/* Descriptors for config file */
 
const parm_desc format_parms[] =
{
    { "spam_header_name",  CP_STRING,	{ &spam_header_name } },
    { "header_format",	   CP_STRING,	{ &header_format } },
    { "terse_format",	   CP_STRING,	{ &terse_format } },
    { "log_header_format", CP_STRING,	{ &log_header_format } },
    { "log_update_format", CP_STRING,	{ &log_update_format } },
    { "spamicity_tags",    CP_FUNCTION,	{ set_spamicity_tags } },
    { "spamicity_formats", CP_FUNCTION,	{ set_spamicity_formats } },

    { NULL,                CP_NONE,	{ (void *) NULL } },
};

/* enums */

enum states {
    S_DEFAULT,
    S_FLAGS,
    S_MIN,
    S_DOT,
    S_PREC,
    S_CONV
};

enum flags {
    F_ZERO = 1,
    F_FP_F = 2,
    F_FP_E = 4,
    F_DELTA = 8,
    F_PREC = 16
};

/* Function Definitions */

/*
    char *tmp = xstrdup(val);
extern char *strsep (char **__restrict __stringp,
		     __const char *__restrict __delim) __THROW;
*/

static bool set_spamicity_fields(pchar *strings, const char *val)
{
    size_t i;
    /* dup the value string and break it up */
    char *tmp = xstrdup(val);
    for (i = 0; i < cls_cnt; i += 1)
    {
	strings[i] = tmp;
	if (*tmp == '\0')
	    continue;
	/* find delimiter */
	while (*tmp != ',' && *tmp != ' ' && *tmp != '\0')
	    tmp += 1;
	/* terminate field */
	*tmp++ = '\0';
	/* skip delimiters */
	while (*tmp == ',' || *tmp == ' ')
	    tmp += 1;
    }
    return true;
}

static bool set_spamicity_tags(const char *val)
{
    bool ok = set_spamicity_fields( spamicity_tags, val );
    return ok;
}

static bool set_spamicity_formats(const char *val)
{
    bool ok = set_spamicity_fields( spamicity_formats, val );
    return ok;
}

static int format_float(char *dest, double src, int min, int prec, int flags, const char *destend)
{
    char buf[20];
    double s;
    int p;
    if (flags & F_DELTA)
	s = 1.0 - src;
    else
	s = src;
    if (flags & F_PREC)
	p = prec;
    else
	if (flags & F_FP_F)
	    p = 6;
	else
	    p = 2;
    if (flags & F_FP_F)
	snprintf (buf, 20, flags & F_ZERO ? "%0*.*f" : "%*.*f", min, p, s);
    else
	snprintf (buf, 20, flags & F_ZERO ? "%0*.*e" : "%*.*e", min, p, s);
    return format_string (dest, buf, 0, 0, 0, destend);
}

static int format_string(char *dest, const char *src, int min, int prec, int flags, const char *destend)
{
    int len = strlen(src);
    if (flags & F_PREC && prec < len)
	len = prec;
    if (dest + len < destend) {
	strncpy(dest, src, len);
    } else {
	fprintf(stderr, "header format is too long.\n");
	exit (2);
    }
    return len;
}

static int format_spamicity(char *dest, const char *fmt, double spamicity, const char *destend)
{
    char temp[20];
    size_t len = sprintf(temp, fmt, spamicity);
    len = format_string(dest, temp, 0, len, 0, destend);
    return len;
}

static void die (const char *msg, ...)
{
    va_list ap;
    va_start (ap, msg);
    vfprintf (stderr, msg, ap);
    va_end (ap);
    exit (2);
}

char *convert_format_to_string(char *buff, size_t size, const char *format)
{
    char *beg = buff;
    char *end = beg + size;
    char temp[20];

    int state = S_DEFAULT;
    int min = 0, prec = 0, flags = 0;

    rc_t status = method->status();
    double spamicity = method->spamicity();

    memset(buff, '\0', size);		/* for debugging */

    while (buff < end && *format)
    {
	switch (state) {
	case S_DEFAULT:
	    if (*format == '%')
		state = S_FLAGS;
	    else
		*buff++ = *format;
	    format++;
	    break;
	case S_FLAGS:
	    switch (*format) {
	    case '0':
		flags |= F_ZERO;
		format++;
		break;
	    case '#':
		if (status == RC_SPAM)
		    flags |= F_DELTA;
		format++;
		break;
	    default:
		state = S_MIN;
		break;
	    }
	    break;
	case S_MIN:
	    if (isdigit (*format))
		min = min * 10 + (*format++ - '0');
	    else
		state = S_DOT;
	    break;
	case S_DOT:
	    if (*format == '.') {
		state = S_PREC;
		format++;
	    } else {
		state = S_CONV;
	    }
	    break;
	case S_PREC:
	    if (isdigit (*format)) {
		prec = prec * 10 + (*format++ - '0');
		flags |= F_PREC;
	    } else {
		state = S_CONV;
	    }
	    break;
	case S_CONV:
	    switch (*format) {
	    case '%':
		*buff++ = '%';
		break;
	    case 'a':		/* a - algorithm, e.g. "graham", "robinson", "fisher" */
		buff += format_string(buff, method->name, 0, prec, flags, end);
		break;
	    case 'c':		/* c - classification, e.g. Yes/No, Spam/Ham/Unsure, or YN, SHU, +-? */
	    {
		const char *val = spamicity_tags[status];
		buff += format_string(buff, val, 0, prec, flags, end);
		break;
	    }
	    case 'd':		/* d - spam/ham as delta, unsure a probability */
	    {
		const char *f = spamicity_formats[status];
		if (status == RC_SPAM)
		    spamicity = 1.0f - spamicity;
		buff += format_spamicity(buff, f, spamicity, end);
		break;
	    }
	    case 'p':		/* p - spamicity as a probability */
	    {
		const char *f = spamicity_formats[status];
		buff += format_spamicity(buff, f, spamicity, end);
		break;
	    }
	    case 'e':		/* e - spamicity as 'e' format */
		flags |= F_FP_E;
		buff += format_float(buff, spamicity, min, prec, flags, end);
		break;
	    case 'f':		/* f - spamicity as 'f' format */
		flags |= F_FP_F;
		buff += format_float(buff, spamicity, min, prec, flags, end);
		break;
	    case 'h':		/* h - spam_header_name, e.g. "X-Bogosity" */
		buff += format_string(buff, spam_header_name, 0, prec, flags, end);
		break;
	    case 'l':		/* l - logging tag */
		buff += format_string(buff, logtag, 0, prec, flags, end);
		break;
	    case 'o':		/* o - spam_cutoff, ex. cutoff=%c */
		buff += format_float(buff, spam_cutoff, min, prec, flags, end);
		break;
	    case 'r':		/* r - run type (s, n, S, or N) */
		snprintf( temp, sizeof(temp), "%c", reg );
		buff += format_string(buff, temp, 0, 0, 0, end);
		break;
	    case 'w':		/* w - word count */
		snprintf( temp, sizeof(temp), "%d", wrdcount );
		buff += format_string(buff, temp, 0, 0, 0, end);
		break;
	    case 'm':		/* m - message count */
		snprintf( temp, sizeof(temp), "%d", msgcount );
		buff += format_string(buff, temp, 0, 0, 0, end);
		break;
	    case 'v':		/* v - version, ex. "version=%v" */
		buff += format_string(buff, VERSION, 0, prec, flags, end);
		break;
	    default:
		die ("unknown header format directive: '%c'\n", *format);
		break;
	    }
	    format += 1;
	    prec = min = flags = 0;
	    state = S_DEFAULT;
	    break;
	default:
	    break;
	}
    }

    *buff = '\0';

    if (DEBUG_FORMAT(0))
	fprintf( stderr, "%s\n", beg );

    return beg;
}

char *format_header(char *buff, size_t size)
{
    return convert_format_to_string( buff, size, header_format );
}

char *format_terse(char *buff, size_t size)
{
    return convert_format_to_string( buff, size, terse_format );
}

char *format_log_update(char *buff, size_t size, char _reg, int _wrd, int _msg)
{
    reg = _reg;
    wrdcount = _wrd;
    msgcount = _msg;
    return convert_format_to_string( buff, size, log_update_format );
}

char *format_log_header(char *buff, size_t size)
{
    return convert_format_to_string( buff, size, log_header_format );
}

/* vim:set ts=8 sw=4 sts=4:*/
