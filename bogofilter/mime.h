/* $Id$ */

/*****************************************************************************

NAME:
   mime.h -- prototypes and definitions for mime.c

******************************************************************************/

#ifndef	HAVE_MIME_H
#define	HAVE_MIME_H

#define MIME_STACK_MAX 100

enum mimetype { MIME_MULTIPART, MIME_MESSAGE, MIME_TEXT, MIME_OTHER };
enum mimeencoding { MIME_7BIT, MIME_8BIT, MIME_BINARY, MIME_QP, MIME_BASE64, MIME_UUENCODE };
enum mimedisposition { MIME_ATTACHMENT, MIME_INLINE };

struct msg_state {
    char *charset;
    char *boundary; /* only valid if mime_type is MIME_MULTIPART or
		       MIME_MESSAGE */
    char *version;
    int mime_header;
    int mime_mail;
    enum mimetype mime_type;
    enum mimeencoding mime_encoding;
    enum mimedisposition mime_disposition;
};

extern int stackp;
extern struct msg_state *msg_state;
extern struct msg_state msg_stack[MIME_STACK_MAX];

void resetmsgstate(struct msg_state *ms, int new);
void mime_boundary(void);
void mime_disposition(void);
void mime_encoding(void);
void mime_type(void);
void mime_version(void);
size_t mime_decode(char *buff, size_t size);

#endif	/* HAVE_MIME_H */
