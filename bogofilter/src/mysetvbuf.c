/*
 * mysetvbuf.c
 * (C) 2002 by Matthias Andree
 * LICENSE: GNU LESSER GENERAL PUBLIC LICENSE
 *
 * offer a consistent wrapper around broken setvbuf calls
 */

#include "config.h"
#include <stdio.h>

int mysetvbuf(FILE *stream, char *buf, int mode , size_t size)
{
#ifdef SETVBUF_REVERSED
	return setvbuf(stream, mode, buf, size);
#else
	return setvbuf(stream, buf, mode, size);
#endif
}
