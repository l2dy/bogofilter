/* $Id$ */

#ifndef COMMON_H_GUARD
#define COMMON_H_GUARD

#if defined(HAVE_LIMITS_H)
#include <limits.h>
#elif defined(HAVE_SYS_PARAM_H)
#include <sys/param.h>
#endif

#include "debug.h"
#include "system.h" /* defines bool */

#define GOODFILE	"goodlist.db"
#define SPAMFILE	"spamlist.db"

#if defined(PATH_MAX)
#define PATH_LEN PATH_MAX
#elif defined(MAXPATHLEN)
#define PATH_LEN MAXPATHLEN
#else
#define PATH_LEN 1024
#endif

#include "globals.h"

#define max(x, y)	(((x) > (y)) ? (x) : (y))
#define min(x, y)	(((x) < (y)) ? (x) : (y))

#undef	HAVE_CHARSET

#ifndef	DISABLE_GRAHAM_METHOD
#define	ENABLE_GRAHAM_METHOD
#endif

#ifndef	DISABLE_ROBINSON_METHOD
#define	ENABLE_ROBINSON_METHOD
#endif

#if	defined(ENABLE_ROBINSON_METHOD) && defined(ENABLE_GRAHAM_METHOD)
#define	GRAHAM_AND_ROBINSON
#endif

#ifndef	ENABLE_ROBINSON_FISHER
#define	ENABLE_ROBINSON_FISHER
#endif

enum dbmode_e { DB_READ = 0, DB_WRITE = 1 };
typedef enum dbmode_e dbmode_t;

typedef enum run_e {
    RUN_NORMAL='r',
    RUN_UPDATE='u',
    REG_SPAM='s', REG_SPAM_TO_GOOD='N', 
    REG_GOOD='n', REG_GOOD_TO_SPAM='S'
} run_t;
extern run_t run_type;

void build_path(char* dest, int size, const char* dir, const char* file);

#endif
