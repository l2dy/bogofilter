/* $Id$ */
/* 
 * $Log$
 * Revision 1.3  2002/10/04 04:01:51  relson
 * Added cvs keywords Id and Log to the files' headers.
 *
 */

#ifndef COMMON_H_GUARD
#define COMMON_H_GUARD

#ifndef __cplusplus
typedef enum bool { FALSE = 0, TRUE = 1 } bool;
#else
const bool FALSE = false;
const bool TRUE = true;
#endif

#define max(x, y)	(((x) > (y)) ? (x) : (y))
#define min(x, y)	(((x) < (y)) ? (x) : (y))

#endif
