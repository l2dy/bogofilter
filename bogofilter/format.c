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

#include "format.h"
#include "bogoconfig.h"
#include "bogofilter.h"
#include "xmalloc.h"
#include "method.h"

/* Function Prototypes */
static int fmtfp(char *dest, double src, int min, int prec, int flags, const char *destend);
static int fmtstr(char *dest, const char *src, int min, int prec, int flags, const char *destend);
static void die (const char *msg, ...);
char *format(const char *frm, double spamicity, rc_t status);

/* initialized static variables */

const char *spam_header_name = SPAM_HEADER_NAME;
const char *header_format = "%h: %y, tests=bogofilter, spamicity=%f, version=%v";
const char *terse_format = "%.1y %f";
const char *check_log_format = "%h: %y, tests=bogofilter, spamicity=%f, version=%v";
const char *update_log_format = "%h: %y, tests=bogofilter, spamicity=%f, version=%v";

const parm_desc format_parms[] =
{
    { "spam_header_name",     CP_STRING,	{ &spam_header_name } },
    { "header_format",        CP_STRING,	{ &header_format } },
    { "terse_format",         CP_STRING,	{ &terse_format } },
    { "update_log_format",    CP_STRING,	{ &update_log_format } },
    { "check_log_format",     CP_STRING,	{ &check_log_format } },
    { NULL,                   CP_NONE,		{ (void *) NULL } },
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

static int fmtfp(char *dest, double src, int min, int prec, int flags, const char *destend)
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
    return fmtstr (dest, buf, 0, 0, 0, destend);
}

static int fmtstr(char *dest, const char *src, int min, int prec, int flags, const char *destend)
{
    int len = strlen(src);
    if (flags & F_PREC && prec < len)
	len = prec;
    if (dest + len < destend) {
	strncpy(dest, src, len);
    } else {
	fprintf(stderr, "header_format is too long.\n");
	exit (2);
    }
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

char *format(const char *frm, double spamicity, rc_t status)
{
    char hdr[256];
    const char *c = frm;
    const char *end = hdr + 256;
    char *s = hdr;
    char *ret;

    int state = S_DEFAULT;
    int min = 0, prec = 0, flags = 0;
    while (s < end && *c)
    {
	switch (state) {
	    case S_DEFAULT:
		if (*c == '%')
		    state = S_FLAGS;
		else
		    *s++ = *c;
		c++;
		break;
	    case S_FLAGS:
		switch (*c) {
		    case '0':
			flags |= F_ZERO;
			c++;
			break;
		    case '#':
			if (status == RC_SPAM)
			    flags |= F_DELTA;
			c++;
			break;
		    default:
			state = S_MIN;
			break;
		}
		break;
	    case S_MIN:
		if (isdigit (*c))
		    min = min * 10 + (*c++ - '0');
		else
		    state = S_DOT;
		break;
	    case S_DOT:
	    	if (*c == '.') {
		    state = S_PREC;
		    c++;
		} else {
		    state = S_CONV;
		}
		break;
	    case S_PREC:
		if (isdigit (*c)) {
		    prec = prec * 10 + (*c++ - '0');
		    flags |= F_PREC;
		} else {
		    state = S_CONV;
		}
		break;
	    case S_CONV:
		switch (*c++) {
		    case '%':
			*s++ = '%';
			break;
		    case 'c':
			s += fmtfp(s, spam_cutoff, min, prec, flags, end);
			break;
		    case 'n':
			*s++ = status == RC_SPAM ? '1' : '0';
			break;
		    case 'f':
			flags |= F_FP_F;
			s += fmtfp(s, spamicity, min, prec, flags, end);
			break;
		    case 'v':
			s += fmtstr(s, VERSION, 0, prec, flags, end);
			break;
		    case 'm':
			s += fmtstr(s, method->name, 0, prec, flags, end);
			break;
		    case 'e':
			flags |= F_FP_E;
			s += fmtfp(s, spamicity, min, prec, flags, end);
			break;
		    case 'y':
			s += fmtstr(s, status == RC_SPAM ? "Yes" : "No", 0, prec, flags, end);
			break;
		    case 'h':
			s += fmtstr(s, spam_header_name, 0, prec, flags, end);
			break;
		    default:
			die ("unknown \'header_format\' directive: %c\n", *c);
			break;
		}
		prec = min = flags = 0;
		state = S_DEFAULT;
		break;
	    default:
		break;
	}
    }

    *s = '\0';
    if ((ret = strdup (hdr)) == NULL)
	xmem_error ("hdr_format");
    else
	return ret;
}
/* vim:set ts=8 sw=4 sts=4:*/
