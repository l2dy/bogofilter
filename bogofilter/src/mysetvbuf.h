#ifndef MYSETVBUF_H
#define MYSETVBUF_H

#include <stddef.h>
#include <stdio.h>

/* from mysetvbuf.c */
int mysetvbuf(FILE *stream, char *buf, int mode, size_t size);

#endif
