/* $Id$ */

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

typedef enum dbmode_e { DB_READ = 0, DB_WRITE = 1 } dbmode_t;

#define PATH_LEN 200
void build_path(char* dest, int size, const char* dir, const char* file);

#endif
