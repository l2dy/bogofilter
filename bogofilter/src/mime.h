/* $Id$ */

/*****************************************************************************

NAME:
   mime.h -- prototypes and definitions for mime.c

******************************************************************************/

#ifndef	HAVE_MIME_H
#define	HAVE_MIME_H

#include "buff.h"
#include "word.h"

#define MIME_STACK_MAX 100

#define HORB(f) (f ? 'h' : 'b')		/* for textual debug messages */

enum mimetype { MIME_TYPE_UNKNOWN, MIME_MULTIPART, MIME_MESSAGE, MIME_TEXT, MIME_TEXT_PLAIN, MIME_TEXT_HTML, MIME_APPLICATION };
enum mimeencoding { MIME_ENCODING_UNKNOWN, MIME_7BIT, MIME_8BIT, MIME_BINARY, MIME_QP, MIME_BASE64, MIME_UUENCODE };
enum mimedisposition { MIME_DISPOSITION_UNKNOWN, MIME_ATTACHMENT, MIME_INLINE };

typedef struct mime_t mime_t;

struct mime_t {
    char *charset;
    char *boundary;	/* only valid if mime_type is MIME_MULTIPART or MIME_MESSAGE */
    size_t boundary_len;
    char *version;
    bool mime_header;
    enum mimetype mime_type;
    enum mimeencoding mime_encoding;
    enum mimedisposition mime_disposition;
    mime_t *parent;
    int  child_count;
};

extern mime_t *msg_state;
extern mime_t *msg_top;

/*
void mime_init(mime_t *parent);
void mime_push(mime_t *parent);
void mime_pop(void);
void mime_free(mime_t *);
bool get_boundary_props(const word_t *boundary, boundary_t *b);
*/

void mime_reset(void);
void mime_add_child(mime_t *parent);
void mime_boundary_set(word_t *text);
bool got_mime_boundary(word_t *text);
void mime_disposition(word_t *text);
void mime_encoding(word_t *text);
void mime_type(word_t *text);
void mime_version(word_t *text);
size_t mime_decode(word_t *buff);
bool mime_is_boundary(word_t *boundary);
enum mimetype get_content_type(void);

#endif	/* HAVE_MIME_H */
