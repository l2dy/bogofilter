/* $Id$ */

/*****************************************************************************

NAME:
   fgetsl.c -- fgets(), returning length.

******************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "fgetsl.h"
#include "word.h"

/* calls exit(2) on read error or when max_size < 2 */
int fgetsl(buff_t *buff, FILE *s)
{
    return xfgetsl(buff, s, 0);
}

int xfgetsl(buff_t *buff, FILE *s, int no_nul_terminate)
{
    int c = 0;

    byte *buf = buff->t.text;
    size_t size = buff->size;

    byte *cp = buf + (buff->read = buff->t.leng);
    int moreroom = no_nul_terminate ? 1 : 0;

    if (size < (size_t) (2 - moreroom)) {
	fprintf(stderr, "Invalid buffer size, exiting.\n");
	abort();
	exit(2);
    }

    if (feof(s))
	return(EOF);

    while (((--size > 0) || (no_nul_terminate && size == 0))
	    && ((c = getc(s)) != EOF)) {
	*cp++ = c;
	if (c == '\n')
	    break;
    }

    if (c == EOF && ferror(s)) {
	perror("stdin");
	exit(2);
    }

    if (!no_nul_terminate)
	*cp = '\0'; /* DO NOT ADD ++ HERE! */
    buff->t.leng = cp - buf;
    return (buff->t.leng - buff->read);
}

#ifdef MAIN
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xmalloc.h>

int main(int argc, char **argv) {
    int size, count;
    buff_t *buff;

    if (argc != 3) {
	fprintf(stderr, "%s <size> <non_nul_terminate>\n", argv[0]);
	exit(2);
    }

    size = atoi(argv[1]);
    buff = buff_new(NULL, size, size);
    buff->t.text = xmalloc(size);
    while(1) {
	count = xfgetsl(buff, stdin, atoi(argv[2]));
	if (count == EOF) break;
	printf("%d ", count);
	if (count > 0) fwrite(buff->t.text, 1, buff->t.leng, stdout);
	putchar('\n');
    }
    xfree(buff->t.text);
    buff_free(buff);
    exit(0);
}
#endif
