/* $Id$ */
/*  constants and declarations for bogofilter */

#ifndef	HAVE_BOGOFILTER_H
#define	HAVE_BOGOFILTER_H

#include <lexer.h>
#include <wordlists.h>

#define GOOD_BIAS	2		// give good words more weight

typedef enum rc_e {RC_SPAM=0, RC_NONSPAM=1}  rc_t;
typedef enum reg_e { REG_NONE = 0, REG_SPAM, REG_GOOD, REG_SPAM_TO_GOOD, REG_GOOD_TO_SPAM } reg_t;

//Represents the secondary data for a word key
typedef struct {
  int freq;
  int msg_freq;
} wordprop_t;

extern int verbose, update;

extern void register_messages(int fd, reg_t register_type);
extern rc_t bogofilter(int fd, double *xss);

#endif	/* HAVE_BOGOFILTER_H */
