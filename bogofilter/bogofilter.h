/* $Id$ */
/*  constants and declarations for bogofilter */

#ifndef	HAVE_BOGOFILTER_H
#define	HAVE_BOGOFILTER_H

#include <lexer.h>
#include <wordlists.h>

enum algorithm { AL_GRAHAM, AL_ROBINSON } algorithm;

extern enum algorithm algorithm;

#define GRAHAM_GOOD_BIAS	2	// don't give good words more weight
#define ROBINSON_GOOD_BIAS	1	// don't give good words more weight
#define GOOD_BIAS (algorithm == AL_GRAHAM ? GRAHAM_GOOD_BIAS : ROBINSON_GOOD_BIAS)

typedef enum rc_e {RC_SPAM=0, RC_NONSPAM=1}  rc_t;

//Represents the secondary data for a word key
typedef struct {
  int freq;
  int msg_freq;
} wordprop_t;

extern int   verbose;

extern rc_t bogofilter(int fd, double *xss);
extern void register_messages(int fd, run_t run_type);
extern void print_bogostats(FILE *fp);

#endif	/* HAVE_BOGOFILTER_H */
