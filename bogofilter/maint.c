/* $Id$ */

/*****************************************************************************

NAME:
   maint.c -- wordlist maintenance functions

AUTHOR:
   David Relson

******************************************************************************/

#include <time.h>
#include <stdlib.h>

#include <config.h>
#include "common.h"

#include "maint.h"

YYYYMMDD today;			/* date as YYYYMMDD */
int	 thresh_count = 0;
YYYYMMDD thresh_date  = 0;
size_t	 size_min = 0;
size_t	 size_max = 0;
bool	 replace_nonascii_characters = false;

/* Function Prototypes */
static YYYYMMDD time_to_date(long days);

/* Function Definitions */

YYYYMMDD time_to_date(long days)
{
    time_t t = time(NULL) - days * 86400;
    struct tm *tm = localtime( &t );
    YYYYMMDD date = (((tm->tm_year + 1900) * 100 + tm->tm_mon + 1) * 100) + tm->tm_mday;
    return date;
}

void set_today(void)
{
    today = time_to_date(0);
}

YYYYMMDD string_to_date(char *s)
{
    YYYYMMDD date = atol((unsigned char *)s);
    if (date < 20020801 && date != 0) {
	date = time_to_date(date);
    }
    return date;
}

/* Keep high counts */
bool check_count(int count)
{
    if (thresh_count == 0)
	return true;
    else {
	bool ok = count > thresh_count;
	return ok;
    }
}

/* Keep recent dates */
bool check_date(int date)
{
    if (thresh_date == 0 || date == 0 || date == today)
	return true;
    else {
	bool ok = thresh_date <= date;
	return ok;
    }
}

/* Keep sizes within bounds */
bool check_size(size_t size)
{
    if (size_min == 0 || size_max == 0)
	return true;
    else {
	bool ok = (size_min <= size) && (size <= size_max);
	return ok;
    }
}

void do_replace_nonascii_characters(byte *str)
{
    byte ch;
    while ((ch=*str) != '\0')
    {
	if ( ch & 0x80)
	    *str = '?';
	str += 1;
    }
}
