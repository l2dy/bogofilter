/* register.h -- constants and declarations for register.c */
/* $Id$ */

#ifndef	REGISTER_H
#define	REGISTER_H

#include "wordhash.h"

extern void register_words(run_t _run_type, wordhash_t *h, int msgcount);
extern void add_hash(wordhash_t *dst, wordhash_t *src);

#endif	/* REGISTER_H */
