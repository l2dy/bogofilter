/* $Id$ */
/*  constants and declarations for bogofilter */

#ifndef	HAVE_BOGOFILTER_H
#define	HAVE_BOGOFILTER_H

#include <wordlists.h>

enum algorithm_e { AL_GRAHAM = 1, AL_ROBINSON };
typedef enum algorithm_e algorithm_t;

extern algorithm_t algorithm;

#define EVEN_ODDS	0.5f		// used for words we want to ignore
#define UNKNOWN_WORD	0.4f		// odds that unknown word is spammish

#define ROBS			0.001f	// Robinson's s
#define ROBX			0.200f	// Robinson's x

#define GRAHAM_GOOD_BIAS	2.0	// don't give good words more weight
#define ROBINSON_GOOD_BIAS	1.0	// don't give good words more weight
#define GOOD_BIAS (algorithm == AL_GRAHAM ? GRAHAM_GOOD_BIAS : ROBINSON_GOOD_BIAS)

typedef enum rc_e {RC_SPAM=0, RC_NONSPAM=1}  rc_t;

//Represents the secondary data for a word key
typedef struct {
  int freq;
  int msg_freq;
} wordprop_t;

extern int   verbose;

extern rc_t bogofilter(int fd, /*@out@*/ double *xss);
extern void register_messages(int fd, run_t run_type);
extern void print_bogostats(FILE *fp, double spamicity);

#endif	/* HAVE_BOGOFILTER_H */
