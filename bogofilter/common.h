/* $Id$ */

#ifndef COMMON_H_GUARD
#define COMMON_H_GUARD

#if defined(HAVE_LIMITS_H)
#include <limits.h>
#elif defined(HAVE_SYS_PARAM_H)
#include <sys/param.h>
#endif

#include "debug.h"

#if defined(PATH_MAX)
#define PATH_LEN PATH_MAX
#elif defined(MAXPATHLEN)
#define PATH_LEN MAXPATHLEN
#else
#define PATH_LEN 1024
#endif

#define max(x, y)	(((x) > (y)) ? (x) : (y))
#define min(x, y)	(((x) < (y)) ? (x) : (y))

#ifndef __cplusplus
typedef enum bool { FALSE = 0, TRUE = 1 } bool;
#else
const bool FALSE = false;
const bool TRUE = true;
#endif

typedef enum dbmode_e { DB_READ = 0, DB_WRITE = 1 } dbmode_t;

typedef enum run_e { RUN_NORMAL=0, RUN_UPDATE, REG_SPAM, REG_GOOD, REG_SPAM_TO_GOOD, REG_GOOD_TO_SPAM } run_t;
extern run_t run_type;

void build_path(char* dest, int size, const char* dir, const char* file);

#endif
