/* $Id$ */

/*****************************************************************************

NAME:
   token.h -- prototypes and definitions for token.c

******************************************************************************/

#ifndef	HAVE_TOKEN_H
#define	HAVE_TOKEN_H

#include "lexer.h"

extern token_t get_token(void);

extern void got_from(void);
extern void got_newline(void);

/* used by lexer_text_html.l */
extern void html_tag(int level);
extern void html_comment(int level);

#endif	/* HAVE_TOKEN_H */
