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
   Gyepi Sam <gyepi@praxis-sw.com>

******************************************************************************/

#include <ctype.h>
#include <stdlib.h>

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

int stackp = -1;

mime_t msg_stack[MIME_STACK_MAX];
mime_t *msg_state = msg_stack;
mime_t *msg_top = msg_stack;

struct type_s
{
  enum mimetype type;
  const char *name;
  size_t len;
}
mime_type_table[] =
{
  { MIME_TEXT_HTML, 	"text/html", 	 9, } ,
  { MIME_TEXT_PLAIN, 	"text/plain", 	10, } ,
  { MIME_TEXT, 		"text/", 	 5, } ,
  { MIME_APPLICATION, 	"application/", 12, } ,
  { MIME_MESSAGE, 	"message/", 	 8, } ,
  { MIME_MULTIPART, 	"multipart/", 	10, } ,
};

struct encoding_s
{
  enum mimeencoding encoding;
  const char *name;
}
mime_encoding_table[] =
{
  { MIME_7BIT, 	   "7BIT", 		} ,
  { MIME_8BIT,     "8BIT", 		} ,
  { MIME_BINARY,   "BINARY", 		} ,
  { MIME_QP,       "QUOTED-PRINTABLE", 	} ,
  { MIME_BASE64,   "BASE64", 		} ,
  { MIME_UUENCODE, "X-UUENCODE", 	} ,
};

struct disposition_s
{
  enum mimedisposition disposition;
  const char *name;
}
mime_disposition_table[] =
{
  { MIME_INLINE,     "inline",     } ,
  { MIME_ATTACHMENT, "attachment", } ,
};

/* boundary properties */
typedef struct {
  bool is_valid;
  bool is_final;
  int depth;
} boundary_t;

/* Function Prototypes */

static const byte *skipws (const byte *t, const byte *e);
static byte *getword (const byte *t, const byte *e);
#if	0
static char *getparam (const byte *t, char *e, const byte *param);
#endif

static void mime_push (mime_t * parent);
static void mime_pop (void);

/* Function Definitions */

#if	0	/* Unused */
const char *mime_type_name(enum mimetype type)
{
  size_t i;
  for (i = 0; i < COUNTOF (mime_type_table); i += 1)
  {
      struct type_s *typ = mime_type_table + i;
      if (typ->type == type)
	  return typ->name;
  }
  return "unknown";
}
#endif

static void
mime_init (mime_t * parent)
{
  msg_header = true;
  msg_state->mime_type = MIME_TEXT;
  msg_state->mime_encoding = MIME_7BIT;
  msg_state->boundary = NULL;
  msg_state->boundary_len = 0;
  msg_state->parent = parent;
  msg_state->charset = xstrdup ("US-ASCII");
  msg_state->child_count = 0;
}

static void
mime_free (mime_t * t)
{
  if (t == NULL)
    return;

  if (t->boundary)
  {
    xfree (t->boundary);
    t->boundary = NULL;
  }

  if (t->charset)
  {
    xfree (t->charset);
    t->charset = NULL;
  }

  if (t->version)
  {
    xfree (t->version);
    t->version = NULL;
  }

  t->parent = NULL;
}

/** Cleanup storage allocation */
void mime_cleanup()
{
    while (stackp > -1)
	mime_pop ();
}

static void
mime_push (mime_t * parent)
{
  if (stackp < MIME_STACK_MAX)
  {
    /* Top level is its own parent, but does not increase child_count */
    if (parent == NULL)
    {
      if (stackp == -1)
	parent = &msg_stack[0];
      else
      {
	fprintf (stderr, "**mime_push: expecting non-null parent\n");
	exit (2);
      }
    }
    else
    {
      parent->child_count++;
    }

    msg_state = &msg_stack[++stackp];

    mime_init (parent);

    if (DEBUG_MIME (1))
      fprintf (dbgout, "*** mime_push. stackp: %d\n", stackp);
  }
  else
  {
    fprintf (stderr, "Attempt to overflow mime stack\n");
  }
}

static void
mime_pop (void)
{
  if (DEBUG_MIME (1))
    fprintf (dbgout, "*** mime_pop. stackp: %d\n", stackp);

  if (stackp > -1)
  {
    mime_t *parent = msg_state->parent;

    if (parent == msg_state)
      parent = NULL;

    mime_free (msg_state);
    stackp--;
    msg_state = stackp == -1 ? NULL : &msg_stack[stackp];
    
    if (msg_state && parent && parent->child_count > 0)
      parent->child_count--;
  }
  else
  {
    fprintf (stderr, "Attempt to underflow mime stack\n");
  }

}

static int
is_mime_container (mime_t * m)
{
  return (m && ((m->mime_type == MIME_MESSAGE) || (m->mime_type == MIME_MULTIPART)));
}

void
mime_reset (void)
{
  if (DEBUG_MIME (1))
    fprintf (dbgout, "*** mime_reset\n");

  while (stackp > -1)
    mime_pop ();

  mime_push (NULL);
}

void
mime_add_child (mime_t * parent)
{
  mime_push (parent);
}

static
bool get_boundary_props(const word_t *boundary, boundary_t *b)
{
  int i;
  const byte *buf = boundary->text;
  size_t blen = boundary->leng;

  b->is_valid = false;

  if (blen > 2 && buf[0] == '-' && buf[1] == '-'){

    while (buf[blen - 1] == '\r' ||
           buf[blen - 1] == '\n')
  	  blen--;

    /* skip initial -- */
    buf += 2;
    blen -= 2;

    /* skip and note ending --, if any */
    if (buf[blen-1] == '-' &&
	buf[blen-2] == '-'){
      b->is_final = true;
      blen -= 2;
    } else {
      b->is_final = false;
    }

    for (i = stackp; i > -1; i--){
      if (is_mime_container (&msg_stack[i]) &&
          msg_stack[i].boundary &&
          (memcmp (msg_stack[i].boundary, buf, blen) == 0)){
	 b->depth = i;
	 b->is_valid = true;
	 break;
      }
    }
  }

  return b->is_valid;
}

bool
mime_is_boundary(word_t *boundary){
    boundary_t b;
    get_boundary_props(boundary, &b);
    return b.is_valid;
}

bool
got_mime_boundary (word_t *boundary)
{
  mime_t *parent;
  boundary_t b;

  get_boundary_props(boundary, &b);

  if (!b.is_valid)
    return false;

  if (DEBUG_MIME (1))
    fprintf (dbgout, "*** got_mime_boundary:  stackp: %d, boundary: '%s'\n",
	     stackp, boundary->text);

  if (stackp > 0)
  {
    /* This handles explicit and implicit boundaries */
    while (stackp > 0 && stackp > b.depth)
      mime_pop ();

    /* explicit end boundary */
    if (b.is_final)
	return true;
  }

  parent = is_mime_container (msg_state) ? msg_state : msg_state->parent;
  mime_push (parent);
  return true;
}

/* skips whitespace, returns NULL when ran into end of string */
static const byte *
skipws (const byte *t, const byte *e)
{
  while (t < e && isspace (*t))
    t++;
  if (t < e)
    return t;
  return NULL;
}

/* skips [ws]";"[ws] */
#if	0
char *
skipsemi (const byte *t, const byte *e)
{
  if (!(t = skipws (t, e)))
    return NULL;
  if (*t == ';')
    t++;
  return skipws (t, e);
}
#endif

/* get next MIME word, NULL when none found.
 * caller must free returned string with xfree() */
static byte *
getword (const byte *t, const byte *e)
{
  int quote = 0;
  int l;
  const byte *ts;
  byte *n;

  t = skipws (t, e);
  if (!t)
    return NULL;
  if (*t == '"') {
    quote++;
    t++;
  }
  ts = t;
  while ((t < e) && (quote ? *t != '"' : !isspace(*t))) {
    t++;
  }
  l = t - ts;
  n = (byte *)xmalloc(l + 1);
  memcpy(n, ts, l);
  n[l] = '\0';
  return n;
}

#if	0
char *
getparam (char *t, char *e, const byte *param)
{
/*    char *w, *u; */

  return NULL;			/* NOT YET IMPLEMENTED */
}
#endif

void
mime_version (word_t *text)
{
  size_t l;
  char *w;

  if (!msg_header)
      return;

  l = strlen("MIME-Version:");
  w = (char *)getword(text->text + l, text->text + text->leng);

  if (!w) return;

  if (msg_state->version)
      xfree(msg_state->version);
  msg_state->version = w;
}

void
mime_disposition (word_t *text)
{
  size_t i;
  size_t l;
  char *w;


  if (!msg_header)
      return;

  l = strlen("Content-Disposition:");
  w = (char *)getword (text->text + l, text->text + text->leng);

  if (!w) return;

  msg_state->mime_disposition = MIME_DISPOSITION_UNKNOWN;
  for (i = 0; i < COUNTOF (mime_disposition_table); i += 1 )
  {
    struct disposition_s *dis = mime_disposition_table + i;
    if (strcasecmp (w, dis->name) == 0)
    {
      msg_state->mime_disposition = dis->disposition;
      if (DEBUG_MIME (1))
	fprintf (dbgout, "*** mime_disposition: %s\n", text->text);
      break;
    }
  }
  if (DEBUG_MIME (1) &&  msg_state->mime_disposition == MIME_DISPOSITION_UNKNOWN)
    fprintf (stderr, "Unknown mime disposition - '%s'\n", w);
  xfree (w);
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

void
mime_encoding (word_t *text)
{
  size_t i;
  size_t l;
  char *w;

  if (!msg_header)
      return;

  l = strlen("Content-Transfer-Encoding:");
  w = (char *)getword(text->text + l, text->text + text->leng);

  if (!w) return;

  msg_state->mime_encoding = MIME_ENCODING_UNKNOWN;
  for (i = 0; i < COUNTOF (mime_encoding_table); i += 1 )
  {
    struct encoding_s *enc = mime_encoding_table + i;
    if (strcasecmp (w, enc->name) == 0)
    {
      msg_state->mime_encoding = enc->encoding;
      if (DEBUG_MIME (1))
	fprintf (dbgout, "*** mime_encoding: %s\n", text->text);
      break;
    }
  }
  xfree (w);
  return;
}

void
mime_type (word_t *text)
{
  size_t l;
  char *w;
  struct type_s *typ;

  if (!msg_header)
      return;

  l = strlen("Content-Type:");
  w = (char *)getword(text->text + l, text->text + text->leng);

  if (!w) return;

  msg_state->mime_type = MIME_TYPE_UNKNOWN;
  for (typ = mime_type_table; typ < mime_type_table + COUNTOF (mime_type_table); typ += 1)
  {
    if (strncasecmp (w, typ->name, typ->len) == 0)
    {
      msg_state->mime_type = typ->type;
      if (DEBUG_MIME (1))
	fprintf (dbgout, "*** mime_type: %s\n", text->text);
      break;
    }
  }
  if (DEBUG_MIME (1) &&  msg_state->mime_type == MIME_TYPE_UNKNOWN)
    fprintf (stderr, "Unknown mime type - '%s'\n", w);
  xfree (w);

  switch (msg_state->mime_type)
  {
  case MIME_TEXT:
  case MIME_TEXT_HTML:
  case MIME_TEXT_PLAIN:
    /* XXX: read charset */
    return;
  case MIME_TYPE_UNKNOWN:
    return;
  case MIME_MULTIPART:
    return;
  case MIME_MESSAGE:
    /* XXX: read boundary */
    return;
  case MIME_APPLICATION:
    /* XXX: read boundary */
    return;
  }
  return;
}

void
mime_boundary_set (word_t *text)
{
  byte *boundary = text->text;
  size_t blen = text->leng;

  if (DEBUG_MIME (1)) {
      int len = blen;
      if (blen > INT_MAX) len = INT_MAX;
      fprintf (dbgout, "*** --> mime_boundary_set: %d '%-.*s'\n", stackp,
	       len, boundary);
  }

  boundary = getword(boundary + strlen("boundary="), boundary + blen);
  if (msg_state->boundary)
      xfree(msg_state->boundary);
  msg_state->boundary = (char *)boundary;
  msg_state->boundary_len = strlen((char *)boundary);

  if (DEBUG_MIME (1))
      fprintf (dbgout, "*** <-- mime_boundary_set: %d '%s'\n", stackp,
	       boundary);

  return;
}

size_t
mime_decode (word_t *text)
{
  size_t count = text->leng;

  if (msg_header)	/* do nothing if in header */
    return count;
  
  /* early out for the uninteresting cases */
  if (msg_state->mime_encoding ==  MIME_7BIT ||
      msg_state->mime_encoding ==  MIME_8BIT ||
      msg_state->mime_encoding ==  MIME_BINARY ||
      msg_state->mime_encoding ==  MIME_ENCODING_UNKNOWN)
      return count;

  if (DEBUG_MIME (3))
    fprintf (dbgout, "*** mime_decode %lu \"%-.*s\"\n", (unsigned long)count, count > INT_MAX ? INT_MAX : (int)(count - 1), text->text);

  /* Do not decode "real" boundary lines */
  if (mime_is_boundary(text) == true)
      return count;

  switch (msg_state->mime_encoding)
  {
  case MIME_QP:
    count = qp_decode(text);
    break;
  case MIME_BASE64:
      if (count > 4)
	  count = base64_decode(text);
    break;
  case MIME_UUENCODE:
    count = uudecode(text);
    break;
  case MIME_7BIT:
  case MIME_8BIT:
  case MIME_BINARY:
  case MIME_ENCODING_UNKNOWN:
    break;
  }

  return count;
}

enum mimetype get_content_type(void)
{
    return msg_state->mime_type;
}
