/* $Id$ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qp.h"

static void die(void) {
    perror("deqp");
    exit(EXIT_FAILURE);
}

int main(void) {
    size_t size;
    char *buf;

    if (fseek(stdin, 0, SEEK_END)) die();
    size = ftell(stdin);
    if (fseek(stdin, 0, SEEK_SET)) die();
    if (!(buf = malloc(size))) die();
    if (fread(buf, 1, size, stdin) != size) die();
    size = qp_decode((unsigned char *)buf, size);
    if (fwrite(buf, 1, size, stdout) != size) die();
    if (fflush(stdout)) die();
    if (fclose(stdout)) die();
    return EXIT_SUCCESS;
}
