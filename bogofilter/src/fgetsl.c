/**
 * \file fgetsl.c Implementation file for NUL-proof fgets() replacement.
 * Also usable as standalone test code when compiled with -DMAIN.
 * \author Matthias Andree
 * \date 2002,2003
 */

#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "fgetsl.h"

/* calls exit(2) on read error or when max_size < 2 */
int fgetsl(char *buf, int max_size, FILE *s)
{
    return xfgetsl(buf, max_size, s, 0);
}

int xfgetsl(char *buf, int max_size, FILE *s, int no_nul_terminate)
{
    int c = 0;
    char *cp = buf;
    char *end = buf + max_size;				/* Physical end of buffer */
    char *fin = end - (no_nul_terminate ? 0 : 1);	/* Last available byte    */

    if (cp >= fin) {
	fprintf(stderr, "Invalid buffer size, exiting.\n");
	abort();
	exit(2);
    }

    if (feof(s))
	return(EOF);

    while ((cp < fin) && ((c = getc(s)) != EOF)) {
	*cp++ = (char)c;
	if (c == '\n')
	    break;
    }

    if (c == EOF && ferror(s)) {
	perror("stdin");
	exit(2);
    }

    if (cp < end)
	*cp = '\0'; /* DO NOT ADD ++ HERE! */
    if (cp == buf && feof(s)) return EOF;
    return(cp - buf);
}

#ifdef MAIN
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xmalloc.h>

int main(int argc, char **argv) {
    char *buf;
    int size, count, non_nul_terminate;

    if (argc != 3) {
	fprintf(stderr, "%s <size> <non_nul_terminate>\n", argv[0]);
	exit(2);
    }

    non_nul_terminate = atoi(argv[2]);
    size = atoi(argv[1]);
    buf = xmalloc(size);
    while(1) {
	count = xfgetsl(buf, size, stdin, non_nul_terminate);
	if (count == EOF) break;
	printf("%d ", count);
	if (count > 0) fwrite((char *)buf, 1, count, stdout);
	if (count > 0 && !non_nul_terminate && buf[count] != '\0') {
	                printf(" (NUL terminator missing)");
	}

	putchar('\n');
    }
    xfree(buf);
    exit(0);
}
#endif
