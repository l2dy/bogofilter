/* $Id$ */

/**
 * \file fgetsl.c
 * fgets() reimplementation that returns the count of octets read,
 * optionally adding a NUL terminator to the buffer.
 */

#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "fgetsl.h"
#include "word.h"

/** This function reads a line from the input stream \a s into the
 * buffer \a buff and NUL-terminates it. It aborts when the buffer capacity is below 2.
 * \returns the count of bytes read, 0 (once directly before EOF) or EOF. */
int fgetsl(buff_t *buff, FILE *s)
{
    return xfgetsl(buff, s, 0);
}

/** This function reads a line from the input stream \a s into the
 * buffer \a buff and NUL-terminates it unless \a no_nul_terminate is
 * set. It aborts when the buffer capacity is insufficient. The buffer
 * capacity is insufficient if it is below 1 when \a no_nul_terminate is
 * set and if it is below 2 when \a no_nul_terminate is unset.  \returns
 * the count of bytes read, 0 (once directly before EOF) or EOF. */
int xfgetsl(buff_t *buff, FILE *s, int no_nul_terminate /** if positive, the buffer is assumed to be a raw string, without NUL termination */)
{
    int c = 0;
    size_t size = buff->size;
    size_t min_size = no_nul_terminate ? 1 : 2;

    byte *cp = buff->t.text;

    if (getenv("BOGOFILTER_ABORT_WHEN_CLOBBER"))
	if (buff->t.leng) {
	    fprintf(stderr, "Nonempty buffer, exiting.\n");
	    abort();
	}

    if (buff->size < min_size) {
	fprintf(stderr, "Invalid buffer size, exiting.\n");
	abort();
	exit(2);
    }

    if (feof(s))
	return(EOF);

    while (size >= min_size && ((c = getc(s)) != EOF)) {
	*cp++ = c;
	size -= 1;
	if (c == '\n')
	    break;
    }

    if (c == EOF && ferror(s)) {
	perror("stdin");
	exit(2);
    }

    if (!no_nul_terminate)
	*cp = '\0'; 		/* DO NOT ADD ++ HERE! */
    buff->t.leng = cp - buff->t.text;
    return buff->t.leng;
}

#ifdef MAIN
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xmalloc.h>

int main(int argc, char **argv) {
    int size, count;
    buff_t *buff;
    int non_nul_terminate;

    if (argc != 3) {
	fprintf(stderr, "%s <size> <non_nul_terminate>\n", argv[0]);
	exit(2);
    }
    
    size = atoi(argv[1]);
    non_nul_terminate = atoi(argv[2]);
    buff = buff_new(NULL, 0, size);
    buff->t.text = xmalloc(size);
    while(1) {
	count = xfgetsl(buff, stdin, non_nul_terminate);
	if (count == EOF) break;
	printf("%d ", count);
	if (count > 0) fwrite(buff->t.text, 1, buff->t.leng, stdout);
	if (count > 0 && !non_nul_terminate && buff->t.text[count] != '\0') {
	    printf("(NUL terminator missing)");
	}
	putchar('\n');
    }
    xfree(buff->t.text);
    buff_free(buff);
    exit(0);
}
#endif
