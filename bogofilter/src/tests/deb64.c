/* $Id$ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

#include "base64.h"

static void die(void) {
    perror("debase64");
    exit(EXIT_FAILURE);
}

int main(void) {
    size_t size;
    word_t *w;

    if (fseek(stdin, 0, SEEK_END)) die();
    size = ftell(stdin);
    if (fseek(stdin, 0, SEEK_SET)) die();
    w = word_new(NULL, size);
    if (fread(w->text, 1, w->leng, stdin) != w->leng) die();
    size = base64_decode(w);
    if (fwrite(w->text, 1, w->leng, stdin) != w->leng) die();
    word_free(w);
    if (fflush(stdout)) die();
    if (fclose(stdout)) die();
    return EXIT_SUCCESS;
}
