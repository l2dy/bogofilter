/* $Id$ */

#ifndef _DEBUG_H
#define _DEBUG_H

#ifdef	NODEBUG
typedef enum bit_e {DEBUG_NONE=0}  bit_t;
#else
#define MASKNAMES "gldswc"
typedef enum bit_e {DEBUG_NONE=0, BIT_GENERAL=1, BIT_LEXER=2, BIT_DATABASE=4, BIT_SPAMICITY=8, BIT_WORDLIST=16, BIT_CONFIG=32}  bit_t;
#endif

extern bit_t debug_mask;

#ifdef	NODEBUG
#define	DEBUG_GENERAL(level) 	0
#define DEBUG_SPAMICITY(level)	0
#define DEBUG_DATABASE(level)	0
#define DEBUG_LEXER(level)	0
#define DEBUG_WORDLIST(level)	0
#define DEBUG_CONFIG(level)	0
#else
#define	DEBUG_GENERAL(level) 	((debug_mask & BIT_GENERAL)   && (verbose > level))
#define DEBUG_SPAMICITY(level)	((debug_mask & BIT_SPAMICITY) && (verbose > level))
#define DEBUG_DATABASE(level)	((debug_mask & BIT_DATABASE)  && (verbose > level))
#define DEBUG_LEXER(level)	((debug_mask & BIT_LEXER)     && (verbose > level))
#define DEBUG_WORDLIST(level)	((debug_mask & BIT_WORDLIST)  && (verbose > level))
#define DEBUG_CONFIG(level)	((debug_mask & BIT_CONFIG)    && (verbose > level))
#endif

void set_debug_mask(const char *mask);

#endif
