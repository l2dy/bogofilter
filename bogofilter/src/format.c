/* $Id$ */

/*****************************************************************************

NAME:
   format.c -- formats a *printf-like string for ouput

Most of the ideas in here are stolen from Mutt's snprintf implementation.

******************************************************************************/

#include "common.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#if NEEDTRIO
#include <trio.h>
#endif

#include "system.h"

#include "format.h"
#include "bogoconfig.h"
#include "bogofilter.h"
#include "method.h"
#include "xstrdup.h"

/* Function Prototypes */
static size_t format_float( char *dest, double val,      int min, int prec, int flags, const char *destend);
static size_t format_string(char *dest, const char *val, int min, int prec, int flags, const char *destend);
static size_t format_spamicity(char *dest, const char *fmt, double spamicity, const char *destend);
static void die (const char *msg, ...);
static char *convert_format_to_string(char *buff, size_t size, const char *format);

/* uninitialized static variables */

const char *reg = "";
const char *unreg = "";
uint wrdcount = 0;
uint msgcount = 0;

/* initialized static variables */

const char *spam_header_name = SPAM_HEADER_NAME;	/* used by lexer */
const char *spam_subject_tag = NULL;			/* used in passthrough mode */
const char *unsure_subject_tag = NULL;			/* used in passthrough mode */

/*
**	formatting characters:
**
**	    h - spam_header_name, e.g. "X-Bogosity"
**
#ifdef	ENABLE_DEPRECATED_CODE
**	    a - algorithm, e.g. "graham", "robinson", "fisher"
**
#endif
**	    c - classification, e.g. Yes/No, Spam/Ham/Unsure, +/-/?
**
**	    D - date, fixed ISO-8601 format for Universal Time ("GMT")
**
**	    e - spamicity as 'e' format
**	    f - spamicity as 'f' format
**	    g - spamicity as 'g' format
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

const char *header_format = "%h: %c, tests=bogofilter, spamicity=%p, version=%v";
const char *terse_format = "%1.1c %f";
const char *log_header_format = "%h: %c, spamicity=%p, version=%v";
const char *log_update_format = "register-%r, %w words, %m messages";

#define RC_COUNT 3
static FIELD spamicity_tags_ynu[RC_COUNT] = { "Yes",   "No",    "Unsure" };
static FIELD spamicity_tags_shu[RC_COUNT] = { "Spam",  "Ham",   "Unsure" };
static FIELD spamicity_format_d[RC_COUNT] = { "%0.6f", "%0.6f", "%0.6f" };

FIELD  *spamicity_tags    = spamicity_tags_ynu;
FIELD  *spamicity_formats = spamicity_format_d;

static bool set_spamicity_tags(const unsigned char *val);
static bool set_spamicity_formats(const unsigned char *val);
static bool set_spamicity_fields(FIELD *strings, const char *val);

/* Descriptors for config file */
 
const parm_desc format_parms[] =
{
    { "spam_header_name",  CP_STRING,	{ &spam_header_name } },
    { "spam_subject_tag",  CP_STRING,	{ &spam_subject_tag } },
    { "unsure_subject_tag",CP_STRING,	{ &unsure_subject_tag } },
    { "header_format",	   CP_STRING,	{ &header_format } },
    { "terse_format",	   CP_STRING,	{ &terse_format } },
    { "log_header_format", CP_STRING,	{ &log_header_format } },
    { "log_update_format", CP_STRING,	{ &log_update_format } },
    { "spamicity_tags",    CP_FUNCTION,	{ (void *)&set_spamicity_tags } },
    { "spamicity_formats", CP_FUNCTION,	{ (void *)&set_spamicity_formats } },

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
    F_ZERO  =  1,
    F_FP_E  =  2,
    F_FP_F  =  4,
    F_FP_G  =  8,
    F_DELTA = 16,
    F_PREC  = 32
};

/* Function Definitions */

void set_terse_mode_format(int mode)
{
    switch (mode) {
    case 1:
	spamicity_tags = spamicity_tags_shu;
	terse_format = "%1.1c %-8.6g";
	break;
    case 2:
	terse_format = "%0.16f";
	break;
    default:
	fprintf(stderr, "Invalid '-T' usage\n");
	exit(EX_ERROR);
    }
}

/* any fields omitted will retain their original value */
static bool set_spamicity_fields(FIELD *fields, const char *val)
{
    size_t i;
    /* dup the value string and break it up */
    char *tmp = xstrdup(val);
    for (i = 0; i < RC_COUNT; i += 1)
    {
	char *tok = strtok(i ? NULL : tmp, " ,");
	if (tok == NULL) break;
	fields[i] = tok;
    }
    return true;
}

static bool set_spamicity_tags(const unsigned char *val)
{
    bool ok = set_spamicity_fields(spamicity_tags, (const char *)val);
    return ok;
}

static bool set_spamicity_formats(const unsigned char *val)
{
    bool ok = set_spamicity_fields(spamicity_formats, (const char *)val);
    return ok;
}

static size_t format_date(char *dest, const char *destend)
{
    struct tm *tm;
    time_t t;
    size_t l;

    /* must check, size_t will wrap to INT_MAX or LONG_MAX otherwise */
    if (dest == destend)
	return 0;

    (void)time(&t);
    tm = gmtime(&t);
    l = strftime(dest, (size_t)(destend - dest - 1), "%Y-%m-%dT%H:%M:%SZ", tm);
    if (l + 1 >= (size_t)(destend - dest)) /* sanitize return */
	l = 0;
    return l;
}

static size_t format_float(char *dest, double src, 
	int min, int prec, int flags, const char *destend)
{
    #define SIZ 20
    char buf[SIZ];
    const char *fmt;
    double s;
    int p;

    s = ! (flags & F_DELTA) ? src : 1.0 - src;
    if (flags & F_DELTA && s > 0.001)
	s = src;
    if (flags & F_PREC)
	p = prec;
    else
	p = (flags & (F_FP_F|F_FP_G)) ? 6 : 2;
    if (flags & F_FP_F)
	fmt = flags & F_ZERO ? "%0*.*f" : "%*.*f";
    else
	fmt = flags & F_ZERO ? "%0*.*e" : "%*.*e";
    if (flags & F_FP_G)
	fmt = flags & F_ZERO ? "%0*.*g" : "%*.*g";
    snprintf (buf, SIZ, fmt, min, p, s);
    return format_string (dest, buf, 0, 0, 0, destend);
}

static size_t format_string(char *dest, const char *src, int min, int prec, int flags, const char *destend)
{
    int s_len = (int) strlen(src);
    int len;
    if (s_len > INT_MAX) {
	fprintf(stderr, "cannot handle string length (%lu) above %d, aborting\n", (unsigned long)s_len, INT_MAX);
	internal_error;
    }
    len = s_len;
    (void)min; /* kill compiler warning */

    if (flags & F_PREC && prec < len)
	len = prec;
    if (dest + len + 1 < destend) {
	memcpy(dest, src, len);
	dest[len] = '\0';
    } else {
	fprintf(stderr, "header format is too long.\n");
	/* abort to obtain a stack backtrace */
	abort();
    }
    return len;
}

static size_t format_spamicity(char *dest, const char *fmt, double spamicity, const char *destend)
{
    char temp[20];
    uint len = snprintf(temp, sizeof(temp), fmt, spamicity);
    if (len > INT_MAX)
	len = INT_MAX; /* clamp */
    len = format_string(dest, temp, 0, len, 0, destend);
    return len;
}

static void die (const char *msg, ...)
__attribute__ ((format(printf, 1, 2))) __attribute__ ((noreturn));
static void die (const char *msg, ...)
{
    va_list ap;
    va_start (ap, msg);
    vfprintf (stderr, msg, ap);
    va_end (ap);
    exit(EX_ERROR);
}

static
char *convert_format_to_string(char *buff, size_t size, const char *format)
{
    char *beg = buff;
    char *end = beg + size;
    char temp[20];

    int state = S_DEFAULT;
    int min = 0;
    int prec = 0;
    int flags = 0;

    rc_t status = (*method->status)();
    double spamicity = (*method->spamicity)();

    memset(buff, '\0', size);		/* for debugging */
    memset(temp, '\0', sizeof(temp));	/* for debugging */

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
	    if ('1' <= *format && *format <= '9')
		format++;
	    else if(*format == '-')
		format++;
	    else
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
	    if (isdigit ((unsigned char)*format))
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
	    if (isdigit ((unsigned char)*format)) {
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
#ifdef	ENABLE_DEPRECATED_CODE
	    case 'a':		/* a - algorithm, e.g. "graham", "robinson", "fisher" */
		buff += format_string(buff, method->name, 0, prec, flags, end);
		break;
#endif
	    case 'c':		/* c - classification, e.g. Yes/No, Spam/Ham/Unsure, or YN, SHU, +-? */
	    {
		const char *val = spamicity_tags[status];
		buff += format_string(buff, val, 0, prec, flags, end);
		break;
	    }
	    case 'd':		/* d - spam/non-spam as delta, unsure a probability */
	    {
		const char *f = spamicity_formats[status];
		if (status == RC_SPAM)
		    spamicity = 1.0 - spamicity;
		buff += format_spamicity(buff, f, spamicity, end);
		break;
	    }
	    case 'D':		/* D - date in ISO8601 format for GMT time zone */
	    {
		buff += format_date(buff, end);
		break;
	    }

	    case 'p':		/* p - spamicity as a probability */
	    {
		const char *f = spamicity_formats[status];
		buff += format_spamicity(buff, f, spamicity, end);
		break;
	    }
	    case 'e':		/* e - spamicity as 'e' format */
		buff += format_float(buff, spamicity, min, prec, flags | F_FP_E, end);
		break;
	    case 'f':		/* f - spamicity as 'f' format */
		buff += format_float(buff, spamicity, min, prec, flags | F_FP_F, end);
		break;
	    case 'g':		/* g - spamicity as 'g' format */
		buff += format_float(buff, spamicity, min, prec, flags | F_FP_G, end);
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
	    case 'r':		/* r - run type (s, n, S, or N) - two parts (reg/unreg)*/
		snprintf( temp, sizeof(temp), "%s%s", reg, unreg );
		buff += format_string(buff, temp, 0, 0, 0, end);
		break;
	    case 'w':		/* w - word count */
		snprintf( temp, sizeof(temp), "%u", wrdcount );
		buff += format_string(buff, temp, 0, 0, 0, end);
		break;
	    case 'm':		/* m - message count */
		snprintf( temp, sizeof(temp), "%u", msgcount );
		buff += format_string(buff, temp, 0, 0, 0, end);
		break;
	    case 'v':		/* v - version, ex. "version=%v" */
		buff += format_string(buff, version, 0, prec, flags, end);
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

char *format_log_update(char *buff, size_t size, const char *_reg, const char *_unreg, uint _wrd, uint _msg)
{
    reg = _reg;
    unreg = _unreg;
    wrdcount = _wrd;
    msgcount = _msg;
    return convert_format_to_string( buff, size, log_update_format );
}

char *format_log_header(char *buff, size_t size)
{
    return convert_format_to_string( buff, size, log_header_format );
}

/* vim:set ts=8 sw=4 sts=4:*/
