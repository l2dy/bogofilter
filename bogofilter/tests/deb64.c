/* $Id$ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base64.h"

static void die(void) {
    perror("debase64");
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
    size = base64_decode((unsigned char *)buf, size);
    if (fwrite(buf, 1, size, stdout) != size) die();
    if (fflush(stdout)) die();
    if (fclose(stdout)) die();
    exit(EXIT_SUCCESS);
}
