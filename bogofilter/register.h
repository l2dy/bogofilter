/* register.h -- constants and declarations for register.c */
/* $Id$ */

#ifndef	HAVE_REGISTER_H
#define	HAVE_REGISTER_H

#include <wordhash.h>

extern void register_messages(run_t _run_type);
extern void register_words(run_t _run_type, wordhash_t *h,
			   int msgcount, int wordcount);

#endif	/* HAVE_REGISTER_H */
