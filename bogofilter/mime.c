/* $Id$ */

/*****************************************************************************

NAME:
   mime.c -- lexer mime processing

NOTES:

    RFC2045:

       Header fields occur in at least two contexts:

	(1)   As part of a regular RFC 822 message header.

	(2)   In a MIME body part header within a multipart construct.

AUTHOR:
   Matthias Andree <matthias.andree@gmx.de>
   David Relson <relson@osagesoftware.com>

******************************************************************************/

#include <ctype.h>

#include <config.h>
#include "common.h"

#include "base64.h"
#include "lexer.h"
#include "mime.h"
#include "qp.h"
#include "uudecode.h"
#include "xstrdup.h"
#include "xmalloc.h"

/* Global Variables */

int stackp = 0;

struct msg_state msg_stack[MIME_STACK_MAX];
struct msg_state *msg_state = msg_stack;

struct type_s { 
    enum mimetype type;
    const char *name; 
    size_t len;
} types[] = { 
    { MIME_TEXT,	"text/",	 5, },
    { MIME_MULTIPART,	"multipart/", 	10, },
    { MIME_MESSAGE,	"message/", 	 8, },
};

struct encoding_s { 
    enum mimeencoding encoding;
    const char *name; 
} encodings[] = {
    { MIME_7BIT,	"7BIT",	  		},
    { MIME_8BIT,	"8BIT",	  		},
    { MIME_BINARY,	"BINARY", 		},
    { MIME_QP,		"QP",	  		},
    { MIME_QP,		"quoted-printable",	},
    { MIME_BASE64,	"BASE64", 		},
    { MIME_UUENCODE,	"UUENCODE", 		},
    { MIME_UUENCODE,	"x-UUENCODE", 		},
};

struct disposition_s { 
    enum mimedisposition disposition;
    const char *name; 
} dispositions[] = { 
    { MIME_INLINE,	"inline",     },
    { MIME_ATTACHMENT,	"attachment", },
};

/* Function Prototypes */

static char *skipws(char *t, char *e);
static char *skipsemi(char *t, char *e);
static char *getmimew(char *t, char *e);
#if	0
static char *getparam(char *t, char *e, const char *param);
#endif

void mime_read_block(const char *boundary);

/* Function Definitions */

void resetmsgstate(struct msg_state *ms, int new)
{
    while (new > 0) {
	struct msg_state *t = ms + new;
	if (t->boundary)
	    xfree(t->boundary);
	if (t->charset)
	    xfree(t->charset);
	if (t->version)
	    xfree(t->version);
	new -= 1;
    }
    ms->mime_mail = 0;
    ms->mime_header = 0;
    ms->mime_type = MIME_TEXT;
    ms->mime_encoding = MIME_7BIT;
    ms->boundary = NULL;
    ms->charset = xstrdup("US-ASCII");
}

/* skips whitespace, returns NULL when ran into end of string */
char *skipws(char *t, char *e)
{
    while(t < e && isspace((byte)*t))
	t++;
    if (t < e) return t;
    return NULL;
}

/* skips [ws]";"[ws] */
char *skipsemi(char *t, char *e)
{
    if (!(t = skipws(t, e))) return NULL;
    if (*t == ';') t++;
    return skipws(t, e);
}

/* get next MIME word, NULL when none found.
 * caller must free returned string with xfree() */
char *getmimew(char *t, char *e)
{
    int quote = 0;
    int l;
    char *ts;
    char *n;
#if	0
    t = strchr(t, ':')+1;	/* skip field name */
#endif
    t = skipws(t, e);
    if (!t) return NULL;
    if (*t == '"') {
	quote++;
	t++;
    }
    ts = t;
    while ((t < e) && (quote ? *t != '"' : !isspace((byte)*t))) {
	t++;
    }
    l = t - ts;
    n = xmalloc(l+1);
    memcpy(n, ts, l);
    n[l] = '\0';
    return n;
}

#if	0
char *getparam(char *t, char *e, const char *param)
{
//    char *w, *u;

    return NULL; /* NOT YET IMPLEMENTED */
}
#endif

void mime_version(void)
{
    size_t l = strlen("MIME-Version:");
    char *w = getmimew(yytext+l, yytext + yyleng);
    msg_state->mime_mail = 1;
    msg_state->mime_header = 1;
    msg_state->version = w;
}

void mime_disposition(void)
{
    size_t l = strlen("Content-Disposition:");
    char *w = getmimew(yytext+l, yytext + yyleng);
    struct disposition_s *dis;
    for (dis = dispositions ; dis < dispositions+COUNTOF(dispositions); dis+= 1) {
	if (strcasecmp(w, dis->name) == 0) {
	    msg_state->mime_disposition = dis->disposition;
	    if (DEBUG_MIME(1)) fprintf(stderr, "*** mime_disposition: %s\n", yytext);
	}
    }
    xfree(w);
}

/*********
**
** RFC2045, Section 6.1.  Content-Transfer-Encoding Syntax
**
**     encoding := "Content-Transfer-Encoding" ":" mechanism
**
**     mechanism := "7bit" / "8bit" / "binary" /
**                  "quoted-printable" / "base64" /
**                  ietf-token / x-token
**
*********/

void mime_encoding(void)
{
    size_t l = strlen("Content-Transfer-Encoding:");
    char *w = getmimew(yytext+l, yytext + yyleng);
    struct encoding_s *enc;
    for (enc = encodings ; enc < encodings+COUNTOF(encodings); enc+= 1) {
	if (strcasecmp(w, enc->name) == 0) {
	    msg_state->mime_encoding = enc->encoding;
	    if (DEBUG_MIME(1)) fprintf(stderr, "*** mime_encoding: %s\n", yytext);
	}
    }
    xfree(w);
    return;
}

void mime_type(void)
{
    size_t l = strlen("Content-Type:");
    char *w = getmimew(yytext+l, yytext + yyleng);
    struct type_s *typ;

    if (!w) return;

    msg_state->mime_type = MIME_OTHER;
    for (typ = types ; typ < types+COUNTOF(types); typ+= 1) {
	if (strncasecmp(w, typ->name, typ->len) == 0) {
	    msg_state->mime_type = typ->type;
	    if (DEBUG_MIME(1)) fprintf(stderr, "*** mime_type: %s\n", yytext);
	}
    }
    xfree(w);

    switch(msg_state->mime_type) {
    case MIME_TEXT:
	/* XXX: read charset */
	return;
    case MIME_OTHER:
	return;
    case MIME_MULTIPART:
	return;
    case MIME_MESSAGE:
	/* XXX: read boundary */
	return;
    }
    /* XXX: incomplete */
    return;
}

void mime_boundary(void)
{
    char *boundary;
    size_t len;
    if (yytext[0] != '-' || yytext[1] != '-' ) {
	len = strlen("boundary=");
	boundary = getmimew(yytext+len, yytext + yyleng);
	msg_state->boundary = boundary;
    }
    else {
	boundary = msg_state->boundary;
	if (boundary == NULL )
	    return;
	len = strlen(boundary);
//	if (strncmp( yytext+2, boundary, strlen(boundary)) == 0)
//	    mime_read_block(boundary);
	    /* verify that it's really a boundary line */
	if (memcmp(yytext+2, boundary, len) == 0) {
	    stackp += 1;
	    msg_state = &msg_stack[stackp];
	    memset(msg_state, 0, sizeof(*msg_state));
	    msg_state->mime_header = 1;
	    msg_state->boundary = boundary;
	}
    }
    if (DEBUG_MIME(1)) fprintf(stderr, "*** mime_boundary: %d  %p '%s'\n", stackp, boundary, boundary);
    return;
}

#if	0
void mime_read_block(const char *boundary)
{
    char buff[1024];
    size_t size = sizeof(buff);
    size_t cnt;
	

    bool is_header = true;

    while ((cnt = fgetsl(buff, size, yyin)) > 0) {
	if (is_header) {
	    fprintf(stderr, "H: %s", buff);
	    textblock_add(head, buff, cnt);
	    if (strlen(buff) == 1)
		is_header = false;
	}
	else {
	    fprintf(stderr, "B: %s", buff);
	    textblock_add(body, buff, cnt);
	}
	if (strcmp(buff, boundary) == 0)
	    break;
    }
    return;
}
#endif

size_t mime_decode(char *buff, size_t size)
{
    size_t count = size;
    static bool base64_initialized = false;

    if (msg_state->mime_header)	/* do nothing if in header */
	return count;

    if (!base64_initialized) {
	base64_init();
	base64_initialized = true;
    }

    switch(msg_state->mime_encoding) {
    case MIME_7BIT:
	/* do nothing */
	break;
    case MIME_8BIT:
	/* do nothing */
	break;
    case MIME_BINARY:
	/* do nothing */
	break;
    case MIME_QP:
	count = qp_decode(buff, size);
	break;
    case MIME_BASE64:
	count = base64_decode(buff, size);
	break;
    case MIME_UUENCODE:
	count = uudecode(buff, size);
	break;
    default:
	fprintf( stderr, "Unknown mime encoding - %d\n", msg_state->mime_encoding);
	exit(2);
	break;
    }
    return count;
}
