/* $Id$ */
// autodaemon.h -- lightweight library for daemon processes

// return codes
#define AD_SUCCESS	0
#define AD_NOSOCKET	-1
#define AD_NOCONNECT	-2
#define AD_NOLISTEN	-3
#define AD_NOBIND	-4
#define AD_NOACCEPT	-5
#define AD_NOFORK	-6
#define AD_NOSETSID	-7
#define AD_SIGNAL	-8

// flags -- only one so far
#define AD_FLAG_NONE		0
#define AD_FLAG_NODEMONIZE	1

int sockgets(int fd, char *ptr, int maxlen);
int sockputs(int fd, char *ptr, int nbytes);
int sockprintf(int sock, const char *fmt, ...);
int autodaemon(char *id, int (*init)(void), 
	       int (*clie)(int fd), int (*serv)(int fd, int initstat));

// End
