/* $Id$ */

/*****************************************************************************

NAME:
   bogotune.h -- definitions and prototypes for bogotune.c

******************************************************************************/

#ifndef BOGOTUNE_H
#define BOGOTUNE_H

/* typedefs */

typedef struct result_s {
    uint idx;
    uint rsi;
    uint mdi;
    uint rxi;
    double rs;
    double md;
    double rx;
    double co;
    uint fp;
    uint fn;
} result_t;

#endif
