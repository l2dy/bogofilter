/* $Id$ */

#ifndef MAINT_H
#define MAINT_H

#include <time.h>

#define	AGE_IS_YDAY
#undef	AGE_IS_YDAY
#undef	AGE_IS_YYYYMMDD
#define	AGE_IS_YYYYMMDD

typedef long YYYYMMDD;		/* date as YYYYMMDD */
extern YYYYMMDD today;		/* date as YYYYMMDD */

extern	int      thresh_count;
extern	YYYYMMDD thresh_date;
extern	size_t   size_min, size_max;
extern	bool     replace_nonascii_characters;

/* Function Prototypes */
bool check_date(int dat);
bool check_count(int cnt);
bool check_size(size_t siz);
void do_replace_nonascii_characters(byte *str);

void set_today(void);
time_t long_to_date(long l);
time_t string_to_date(char *s);

#endif	/* MAINT_H */
