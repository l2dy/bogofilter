#ifndef MXCAT_H
#define MXCAT_H

/** mxcat allocates a new buffer and concatenates all strings on the
  * command line into the new buffer. The variable argument list
  * MUST be terminated by a NULL.
  */
char *mxcat(const char *first, ...);

#endif
