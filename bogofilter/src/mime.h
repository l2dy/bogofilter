/* $Id$ */

/** \file mime.h
 * prototypes and definitions for mime.c
 */

#ifndef	HAVE_MIME_H
#define	HAVE_MIME_H

#include "buff.h"
#include "word.h"

#define MIME_STACK_MAX 100

enum mimetype { MIME_TYPE_UNKNOWN, MIME_MULTIPART, MIME_MESSAGE, MIME_TEXT, MIME_TEXT_PLAIN, MIME_TEXT_HTML, MIME_APPLICATION, MIME_IMAGE };
enum mimeencoding { MIME_ENCODING_UNKNOWN, MIME_7BIT, MIME_8BIT, MIME_BINARY, MIME_QP, MIME_BASE64, MIME_UUENCODE };
enum mimedisposition { MIME_DISPOSITION_UNKNOWN, MIME_ATTACHMENT, MIME_INLINE };

typedef struct mime_t mime_t;

struct mime_t {
    char *charset;
    char *boundary;	/* only valid if mime_type is MIME_MULTIPART or MIME_MESSAGE */
    size_t boundary_len;
    enum mimetype mime_type;
    enum mimeencoding mime_encoding;
    enum mimedisposition mime_disposition;
    mime_t *parent;
};

extern mime_t *msg_state;
extern mime_t *msg_top;

void mime_reset(void);
void mime_add_child(mime_t *parent);
void mime_boundary_set(word_t *text);
bool got_mime_boundary(word_t *text);
void mime_content(word_t *text);
uint mime_decode(word_t *buff);
bool mime_is_boundary(word_t *boundary);
enum mimetype get_content_type(void);

void mime_cleanup(void);

void mime_type2(word_t * text);

#endif	/* HAVE_MIME_H */
