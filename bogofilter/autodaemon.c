/* $Id$ */
/*
 * $Log$
 * Revision 1.2  2002/09/23 11:31:53  m-a
 * Unnest comments, and move $ line down by one to prevent CVS from adding nested comments again.
 *
 * Revision 1.1.1.1  2002/09/14 22:15:20  adrian_otto
 * 0.7.3 Base Source
 * */
/*****************************************************************************

NAME:
   autodaemon.c -- self-starting daemon using Unix-domain sockets

ENTRY POINTS:
   autodaemon() -- framework for programming lightweight daemons
   daemonize()  -- make the current process a daemon

THEORY
   Consider the following problem: your program is called frequently to
answer some query.  Initializing the database to answer the question is
expensive, but answering questions is cheap.  You don't want to pay the
startup overhead on each call.  
   Solution: autodaemon.  The first time this code is called, it forks
a lighweight TCP/IP server as a daemon.  The daemon does the expensive
startup *once*.  All subsequent calls to the service query the daemon.
   With suitable pains taken, the process-hacking stuff in here could
be made portable to decade-old Unix boxes; I've done it before.  But
bugger that for a lark.  It's all about POSIX interfaces and Linux
now, baby.

AUTHOR:
   Eric S. Raymond <esr@thyrsus.com>, August 2002.  This source code
example and its associated .h file are part of bogofilter and the Unix
Cookbook, and are released under the MIT license.  Compile with -DMAIN
to build the demonstrator.

******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>	/* get umask(2) prototyped */
#include <setjmp.h>
#include <sys/resource.h>
#include <stdarg.h>

#include "autodaemon.h"

int daemonize(void)
/* detach from control TTY, become process group leader, catch SIGCHLD */
{
    int fd;
    pid_t childpid;
    struct rlimit	limit;

    // if we are started by init (process 1) via /etc/inittab,
    // we needn't bother to detach from our process group context
    if (getppid() != 1) 
    {
	struct sigaction sa_new;

	/* Ignore BSD terminal stop signals */
	memset (&sa_new, 0, sizeof sa_new);
	sigemptyset (&sa_new.sa_mask);
	sa_new.sa_handler = SIG_IGN;
#ifdef 	SIGTTOU
	sigaction (SIGTTOU, &sa_new, NULL);
#endif
#ifdef	SIGTTIN
	sigaction (SIGTTIN, &sa_new, NULL);
#endif
#ifdef	SIGTSTP
	sigaction (SIGTSTP, &sa_new, NULL);
#endif

	// In case we were not started in the background, fork and let
	// the parent exit.  Guarantees that the child is not a process
	// group leader
	if ((childpid = fork()) < 0) {
	    return(AD_NOFORK);
	}
	else if (childpid > 0) 
	    exit(0);  /* parent */
  
	// Make ourselves the leader of a new process group with no
	// controlling terminal.
	if (setsid() < 0) {
	    return(AD_NOSETSID);
	}
  
	// lose controlling tty
	sigaction (SIGHUP, &sa_new, NULL);
	if ((childpid = fork()) < 0) {
	    return(AD_NOFORK);
	}
	else if (childpid > 0) {
	    exit(0); 	/* parent */
	}

    }

    // Close any/all open file descriptors
    if (getrlimit(RLIMIT_NOFILE, &limit) > -1)
	for (fd = limit.rlim_cur;  fd >= 0;  fd--)
	    close(fd);	// not checking this should be safe, no writes

    // OK for these to fail, we're probably not going to use them anyway
    fd = open("/dev/null", O_RDONLY);	// stdin to /dev/null
    dup(fd);				// stdout to /dev/null
    dup(fd);				// stderr to /dev/null

    // move to root directory, so we don't prevent filesystem unmounts
    chdir("/");

    // set our umask to something reasonable (we hope) */
    umask(022);

    return(AD_SUCCESS);
}

// The rest of the world.

int sockgets(int fd, char *ptr, int maxlen)
// equivalent of fgets(3) when fd is a stream socket
{
    int n, rc;
    char c;

    for (n = 1; n < maxlen; n++) {
	if ( (rc = read(fd, &c, 1)) == 1) {
	    *ptr++ = c;
	    if (c == '\n')
		break;
	} else if (rc == 0) {
	    if (n == 1)
		return(0);	// EOF, no data read
	    else
		break;		// EOF, some data was read
	} else
	    return(-1);
    }
  
    *ptr = '\0';

    return(n);
}

int sockputs(int fd, char *ptr, int nbytes) 
// substitute for write(2) when fd is a stream socket */
{
  int nleft, nwritten;

  nleft = nbytes;
  while (nleft > 0) {
    nwritten = write(fd, ptr, nleft);
    if (nwritten <= 0)
	return(nwritten);		//
    
    nleft -= nwritten;
    ptr   += nwritten;
  }
  return(nbytes - nleft);
}

int sockprintf(int sock, const char *fmt, ... )
/* assemble command in printf(3) style and send to the server */
{
    char buf[BUFSIZ];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    sockputs(sock, buf, strlen(buf));
}


//
// Entry points begin here 
//

#define THROW_SIGPIPE	1

static jmp_buf	restart;

static void reaper(int sig)
// reap children's return codes to prevent zombies
{
    int status = 0;

    while (waitpid(-1, &status, WNOHANG) > 0)
	continue;

    // in case you have old-style resetting signals
    signal(SIGCHLD, reaper);
}

static void sigpipe_handler (int signal)
// handle SIGPIPE signal indicating broken stream socket or terminated server
{
    longjmp(restart, THROW_SIGPIPE);
}

static int client(char *id, int (*hook)(int fd))
{
    struct sockaddr_un serv_addr;
    int status, js, client_socket;

    // Open a TCP socket (a unix domain socket).
    if ( (client_socket = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) {
	return(AD_NOSOCKET);	// errno will be set
    }
  
    // Yes, we *want* byte zero of the pathname to be a NUL.
    // This invokes the obscure private socket namespace feature,
    // so we don't have to have a socket hanging out in the
    // filesystem as an i-node.
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sun_family = AF_UNIX;
    strcpy(serv_addr.sun_path+1, id);
  
    // Connect to the server.
    if (connect(client_socket, (struct sockaddr *)&serv_addr,
		sizeof(serv_addr)) < 0) {
	return(AD_NOCONNECT);	// errno will be set
    }
 
    // Arrange to croak gracefully if server dies
    signal(SIGPIPE, sigpipe_handler);

    // run interpretation
    if ((js = setjmp(restart)) == 0)
	status = hook(client_socket);
    else
	status = AD_SIGNAL;
 

    close(client_socket);
    return(status);
}

static int server(char *id, int (*hook)(int fd, int initstat), int initstat)
{
    int sockfd, clilen, childpid, server_socket;
    struct sockaddr_un cli_addr, serv_addr;

     // Open a TCP socket (an Internet stream socket).
    if ( (sockfd = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) {
	return(AD_NOSOCKET);	// errno will be set
    }
  
    // Bind our local address so that the client can send to us.
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sun_family      = AF_UNIX;
    strcpy(serv_addr.sun_path+1, id);

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
	return(AD_NOBIND);	// errno will be set
    }

    if (listen(sockfd, 5) < 0) {
	return(AD_NOLISTEN);	// errno will be set
    }

    signal(SIGCHLD, reaper);

    for ( ; ; ) {
	// Quietly dispatch yourself after half an hour of no requests.
	// Next client call will restart.
	alarm(30 * 60);

	// Wait for a connection from a client process.
	// Each client will get a new process instance.
	clilen = sizeof(cli_addr);
	server_socket = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
	if (server_socket < 0) {
	    return(AD_NOACCEPT);	// errno will be set
	}
    
	if ( (childpid = fork()) < 0) {
	    return(AD_NOFORK);		// errno will be set
	} else if (childpid == 0) {	// child process
	    close(sockfd);		// close original socket

	    // run interpretation
	    _exit(hook(server_socket, 0));
	}

	close(server_socket);		// parent process
    }
    return(AD_SUCCESS);
}

int autodaemon(char *id, int (*init)(void),
	       int (*clie)(int fd),
	       int (*serv)(int fd, int initstat))
// Talk to a daemon version of ourself.  Fork one if it doesn't exist
{
    int status;

    // Try to connect to an existing server
    status = client(id, clie);
    if (status == AD_SUCCESS)
	return(status);

    // No existing server?  Maybe we should spawn one.
    if (status == AD_NOCONNECT)
    {
	int childpid;

	// Has to go before for to avoid a race condition
	if (childpid = fork())
	{
	    sleep(3);
	    return client(id, clie);
	}
	else
	{
	    int initstat = 0;

	    // Expensive initialization goes here.  Resulting data in core
	    // will be shared by all spawned children.
	    if (init)
		initstat = init();
	    daemonize();
	    _exit(server(id, serv, initstat));
	}
    }
    else
	// Can't do anything right
	return(status);
}

#ifdef MAIN
#include <ctype.h>

#define MAXLINE	2048

int server_guts(int fd, int initstat)
// sample server-side interpreter -- all-caps the inputs.
{
    int n;
    char line[MAXLINE], *cp;
    static int counter;

    for ( ; ; ) {
	n = sockgets(fd, line, MAXLINE);
	if (n == 0)
	    return;		/* connection terminated */
	else if (n < 0) 
	    continue;

	for (cp = line; *cp; cp++)
	    *cp = toupper(*cp);
	sprintf(line + strlen(line) - 1, " pid=%d, ppid=%d, count=%d\n", getpid(), getppid(), counter++);
    
	sockputs(fd, line, strlen(line));
    }

    return(AD_SUCCESS);
}

int client_guts(int fd)
// sample client-side interpreter -- ship an input line, get back a line.
{
    int n;
    char sendline[MAXLINE], recvline[MAXLINE + 1];

    fprintf(stderr, "autodaemon client: started up (%d)\n", getpid());
    while (fgets(sendline, MAXLINE, stdin) != NULL) 
    {
	// Ship the line down
	n = strlen(sendline);
	if (sockputs(fd, sendline, n) != n) 
	    fprintf(stderr, "client_guts: write error on socket\n");
      
	// Now read a line from the socket and report it.
	n = sockgets(fd, recvline, MAXLINE);
	if (n < 0)
	    fprintf(stderr, "autodaemon client: sockgets error\n");
	else if (n == 0)
	{
	    // This is a bit shaky.  In theory, the server should never
	    // return 0 bytes if it's still alive, because we gather 
	    // characters to the next newline.
	    fprintf(stderr, "autodaemon client: server appears defunct\n");
	    break;
	}
	else
	    fprintf(stderr, "autodaemon client: server sent %d bytes, errno %d: %s", 
		n, errno, recvline);
    }
  
    if (ferror(stdin))
	fprintf(stderr, "autodaemon client: error reading stdin\n");
    fprintf(stderr, "autodaemon client: signing off (%d)\n", getpid());
    return(AD_SUCCESS);
}

int main(int argc, char **argv)
{
    int status = autodaemon("autodaemon", NULL, client_guts, server_guts);
    if (status)
	fprintf(stderr, "Error %d, %s\n", status, strerror(errno)); 
    exit(0);
}

#endif /* MAIN */
