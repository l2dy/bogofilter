/* $Id$ */

/*****************************************************************************

NAME:
   common.h -- common definitions and prototypes for bogofilter

******************************************************************************/

#ifndef COMMON_H
#define COMMON_H

#include "config.h"

#if defined(HAVE_LIMITS_H)
#include <limits.h>
#elif defined(HAVE_SYS_PARAM_H)
#include <sys/param.h>
#endif

#include <stdio.h>

#include "debug.h"
#include "system.h"	/* defines bool, uint32_t */

/* length of token will not exceed this... */
#define MAXTOKENLEN	30

#define WORDLIST	"wordlist" DB_EXT
#define GOODFILE	"goodlist" DB_EXT
#define SPAMFILE	"spamlist" DB_EXT
#define IGNOREFILE	"ignorelist" DB_EXT

#define max(x, y)	(((x) > (y)) ? (x) : (y))
#define min(x, y)	(((x) < (y)) ? (x) : (y))

#if defined(PATH_MAX)
#define PATH_LEN PATH_MAX
#elif defined(MAXPATHLEN)
#define PATH_LEN MAXPATHLEN
#else
#define PATH_LEN 1024
#endif

/* Default build includes Graham, Robinson, and Robinson-Fisher methods */

#if	defined(ENABLE_ROBINSON_METHOD) || defined(ENABLE_ROBINSON_FISHER)
#define	ROBINSON_OR_FISHER
#endif

#if	defined(ENABLE_GRAHAM_METHOD) && defined(ROBINSON_OR_FISHER)
#define	GRAHAM_AND_ROBINSON
#endif

#define max(x, y)	(((x) > (y)) ? (x) : (y))
#define min(x, y)	(((x) < (y)) ? (x) : (y))

#define COUNTOF(array)	(size_t)(sizeof(array)/sizeof(array[0]))

typedef unsigned char byte;

enum dbmode_e { DB_READ = 0, DB_WRITE = 1 };
typedef enum dbmode_e dbmode_t;

#define BIT(n)	(1 << n)

typedef enum run_e {
    RUN_UNKNOWN= 0,
    RUN_NORMAL = BIT(0),
    RUN_UPDATE = BIT(1),
    REG_SPAM   = BIT(2),
    REG_GOOD   = BIT(3),
    UNREG_SPAM = BIT(4),
    UNREG_GOOD = BIT(5)
} run_t;
extern run_t run_type;
extern bool  run_classify;
extern bool  run_register;

typedef struct {
    double mant;
    int    exp;
} FLOAT;

typedef enum priority_e {
    PR_NONE,		/* 0 */
    PR_ENV_HOME,	/* 1 */
    PR_CFG_SITE,	/* 2 */
    PR_CFG_USER,	/* 3 */
    PR_CFG_UPDATE,	/* 4 */
    PR_ENV_BOGO,	/* 5 */
    PR_COMMAND		/* 6 */
} priority_t;

typedef enum bulk_e {
    B_NORMAL,
    B_CMDLINE,
    B_STDIN
} bulk_t;

typedef enum wl_e { W_SEPARATE=1,
		    W_COMBINED=2 } wl_t;

#include "globals.h"

/* Represents the secondary data for a word key */
typedef struct {
    int freq;
    long good;
    long bad;
    double prob;
} wordprop_t;

extern int build_path(char* dest, size_t size, const char* dir, const char* file);

#define internal_error do { fprintf(stderr, "Internal error in %s:%u\n", __FILE__, __LINE__); abort(); } while(0)

#endif
