/* register.h -- constants and declarations for register.c */
/* $Id$ */

#ifndef	REGISTER_H
#define	REGISTER_H

#include <wordhash.h>

extern void register_messages(run_t _run_type);
extern void register_words(run_t _run_type, wordhash_t *h, int msgcount);

#endif	/* REGISTER_H */
