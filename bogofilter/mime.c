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
types[] =
{
  { MIME_TEXT_HTML, 	"text/html", 	9,} ,
  { MIME_TEXT_PLAIN, 	"text/plain", 	10,} ,
  { MIME_TEXT, 		"text/", 	5,} ,
  { MIME_APPLICATION, 	"application/", 12,} ,
  { MIME_MESSAGE, 	"message/", 	8,} ,
  { MIME_MULTIPART, 	"multipart/", 	10,},
};

struct encoding_s
{
  enum mimeencoding encoding;
  const char *name;
}
encodings[] =
{
  { MIME_7BIT, 	   "7BIT",} ,
  { MIME_8BIT,     "8BIT",} ,
  { MIME_BINARY,   "BINARY",} ,
  { MIME_QP,       "QUOTED-PRINTABLE",} ,
  { MIME_BASE64,   "BASE64",} ,
  { MIME_UUENCODE, "X-UUENCODE",} ,
};

struct disposition_s
{
  enum mimedisposition disposition;
  const char *name;
}
dispositions[] =
{
  { MIME_INLINE, "inline",} ,
  { MIME_ATTACHMENT, "attachment",} ,
};

/* boundary properties */
typedef struct {
  bool is_valid;
  bool is_final;
  int depth;
} boundary_t;

/* Function Prototypes */

static const byte *skipws (const byte *t, const byte *e);
static char *getword (const byte *t, const byte *e);
#if	0
static char *getparam (const byte *t, char *e, const byte *param);
#endif

/* Function Definitions */

#if	0	/* Unused */
const char *mime_type_name(enum mimetype type)
{
  struct type_s *typ;
  for (typ = types; typ < types + COUNTOF (types); typ += 1)
  {
      if (typ->type == type)
	  return typ->name;
  }
  return "unknown";
}
#endif

static void
mime_init (mime_t * parent)
{
  msg_state->mime_header = true;
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
  msg_header = true;		/*FIXME: fold this into mime structure */
}

void
mime_add_child (mime_t * parent)
{
  mime_push (parent);
}

static
bool get_boundary_props(const byte *boundary, int boundary_len, boundary_t *b)
{
  int i;

  b->is_valid = false;

  if (boundary_len > 2 && *(boundary) == '-' && *(boundary + 1) == '-'){

    while (*(boundary + boundary_len - 1) == '\r' ||
           *(boundary + boundary_len - 1) == '\n')
  	  boundary_len--;

    /* skip initial -- */
    boundary += 2;
    boundary_len -= 2;

    /* skip and note ending --, if any */
    if (*(boundary + boundary_len - 1) == '-'
	    && *(boundary + boundary_len - 2) == '-'){
      b->is_final = true;
      boundary_len -= 2;
    } else {
      b->is_final = false;
    }

    for (i = stackp; i > -1; i--){
      if (is_mime_container (&msg_stack[i]) &&
          msg_stack[i].boundary &&
          (memcmp (msg_stack[i].boundary, boundary, boundary_len) == 0)){
	 b->depth = i;
	b->is_valid = true;
	 break;
      }
    }
  }

  return b->is_valid;
}

bool
mime_is_boundary(const byte *boundary, int len){
    boundary_t b;
    get_boundary_props(boundary, len, &b);
    return b.is_valid;
}

bool
got_mime_boundary (const byte *boundary, int boundary_len)
{
  mime_t *parent;
  boundary_t b;

  get_boundary_props(boundary, boundary_len, &b);

  if (!b.is_valid)
    return false;

  if (DEBUG_MIME (1))
    fprintf (dbgout, "*** got_mime_boundary:  stackp: %d, boundary: '%s'\n",
	     stackp, boundary);

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
static char *
getword (const byte *t, const byte *e)
{
  int quote = 0;
  int l;
  const byte *ts;
  char *n;

  t = skipws (t, e);
  if (!t)
    return NULL;
  if (*t == '"')
  {
    quote++;
    t++;
  }
  ts = t;
  while ((t < e) && (quote ? *t != '"' : !isspace (*t)))
  {
    t++;
  }
  l = t - ts;
  n = xmalloc (l + 1);
  memcpy (n, ts, l);
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
mime_version (const byte *text, int leng)
{
  size_t l;
  char *w;

  if (!msg_header && !msg_state->mime_header)
      return;

  l = strlen ("MIME-Version:");
  w = getword (text + l, text + leng);

  if (!w) return;

  msg_state->version = w;
}

void
mime_disposition (const byte *text, int leng)
{
  size_t l;
  char *w;
  struct disposition_s *dis;

  if (!msg_header && !msg_state->mime_header)
      return;

  l = strlen ("Content-Disposition:");
  w = getword (text + l, text + leng);

  if (!w) return;

  msg_state->mime_disposition = MIME_DISPOSITION_UNKNOWN;
  for (dis = dispositions; dis < dispositions + COUNTOF (dispositions);
       dis += 1)
  {
    if (strcasecmp (w, dis->name) == 0)
    {
      msg_state->mime_disposition = dis->disposition;
      if (DEBUG_MIME (1))
	fprintf (dbgout, "*** mime_disposition: %s\n", text);
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
mime_encoding (const byte *text, int leng)
{
  size_t l;
  char *w;
  struct encoding_s *enc;

  if (!msg_header && !msg_state->mime_header)
      return;

  l = strlen ("Content-Transfer-Encoding:");
  w = getword (text + l, text + leng);

  if (!w) return;

  msg_state->mime_encoding = MIME_ENCODING_UNKNOWN;
  for (enc = encodings; enc < encodings + COUNTOF (encodings); enc += 1)
  {
    if (strcasecmp (w, enc->name) == 0)
    {
      msg_state->mime_encoding = enc->encoding;
      if (DEBUG_MIME (1))
	fprintf (dbgout, "*** mime_encoding: %s\n", text);
      break;
    }
  }
  xfree (w);
  return;
}

void
mime_type (const byte *text, int leng)
{
  size_t l;
  char *w;
  struct type_s *typ;

  if (!msg_header && !msg_state->mime_header)
      return;

  l = strlen ("Content-Type:");
  w = getword (text + l, text + leng);

  if (!w) return;

  msg_state->mime_type = MIME_TYPE_UNKNOWN;
  for (typ = types; typ < types + COUNTOF (types); typ += 1)
  {
    if (strncasecmp (w, typ->name, typ->len) == 0)
    {
      msg_state->mime_type = typ->type;
      if (DEBUG_MIME (1))
	fprintf (dbgout, "*** mime_type: %s\n", text);
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
mime_boundary_set (const byte *text, int leng)
{
  char *boundary;

  if (DEBUG_MIME (1))
    fprintf (dbgout, "*** --> mime_boundary_set: %d '%-.*s'\n", stackp,
	     leng, text);

  boundary = getword (text + strlen ("boundary="), text + leng);
  msg_state->boundary = boundary;
  msg_state->boundary_len = strlen (boundary);

  if (DEBUG_MIME (1))
    fprintf (dbgout, "*** <-- mime_boundary_set: %d '%s'\n", stackp,
	     boundary);

  return;
}

size_t
mime_decode (byte *buff, size_t size)
{
  size_t count = size;

  if (msg_state->mime_header)	/* do nothing if in header */
    return count;
  
  /* early out for the uninteresting cases */
  if (msg_state->mime_encoding ==  MIME_7BIT ||
      msg_state->mime_encoding ==  MIME_8BIT ||
      msg_state->mime_encoding ==  MIME_BINARY ||
      msg_state->mime_encoding ==  MIME_ENCODING_UNKNOWN)
      return count;

  if (DEBUG_MIME (3))
    fprintf (dbgout, "*** mime_decode %lu \"%-.*s\"\n", (unsigned long)size, size > INT_MAX ? INT_MAX : (int)(size - 1), buff);

  /* Do not decode "real" boundary lines */
  if (mime_is_boundary(buff, size) == true)
      return count;

  switch (msg_state->mime_encoding)
  {
  case MIME_QP:
    count = qp_decode(buff, size);
    break;
  case MIME_BASE64:
    if (size > 4)
      count = base64_decode(buff, size);
    break;
  case MIME_UUENCODE:
    count = uudecode(buff, size);
    break;
  case MIME_7BIT:
  case MIME_8BIT:
  case MIME_BINARY:
  case MIME_ENCODING_UNKNOWN:
    break;
  }

  return count;
}
