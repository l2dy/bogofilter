/* $Id$ */

#ifndef _DEBUG_H
#define _DEBUG_H

#define	DEBUG_NONE	0

#ifndef	NODEBUG
#define BIT_NAMES	"cdfglmstw"
#define BIT_CONFIG	( 1 << ('C' - 'A'))
#define BIT_DATABASE	( 1 << ('D' - 'A'))
#define BIT_FORMAT	( 1 << ('F' - 'A'))
#define BIT_GENERAL	( 1 << ('G' - 'A'))
#define BIT_LEXER	( 1 << ('L' - 'A'))
#define BIT_MIME	( 1 << ('M' - 'A'))
#define BIT_SPAMICITY	( 1 << ('S' - 'A'))
#define BIT_TEXT	( 1 << ('T' - 'A'))
#define BIT_WORDLIST	( 1 << ('W' - 'A'))
#endif

extern FILE *dbgout;
extern int debug_mask;

#ifdef	NODEBUG
#define	DEBUG_GENERAL(level)	0
#define DEBUG_CONFIG(level)	0
#define DEBUG_DATABASE(level)	0
#define DEBUG_FORMAT(level)	0
#define DEBUG_LEXER(level)	0
#define DEBUG_MIME(level)	0
#define DEBUG_SPAMICITY(level)	0
#define DEBUG_TEXT(level)	0
#define DEBUG_WORDLIST(level)	0
#else
#define	DEBUG_GENERAL(level)	((debug_mask & BIT_GENERAL)   && (verbose > level))
#define DEBUG_CONFIG(level)	((debug_mask & BIT_CONFIG)    && (verbose > level))
#define DEBUG_DATABASE(level)	((debug_mask & BIT_DATABASE)  && (verbose > level))
#define DEBUG_FORMAT(level)	((debug_mask & BIT_FORMAT)    && (verbose > level))
#define DEBUG_LEXER(level)	((debug_mask & BIT_LEXER)     && (verbose > level))
#define DEBUG_MIME(level)	((debug_mask & BIT_MIME)      && (verbose > level))
#define DEBUG_SPAMICITY(level)	((debug_mask & BIT_SPAMICITY) && (verbose > level))
#define DEBUG_TEXT(level)	((debug_mask & BIT_TEXT)      && (verbose > level))
#define DEBUG_WORDLIST(level)	((debug_mask & BIT_WORDLIST)  && (verbose > level))
#endif

void set_debug_mask(const char *mask);

#endif
