#ifndef DB_LOCK_H
#define DB_LOCK_H

#include <sys/types.h> /* for off_t */

/* function prototypes */

int	set_lock(const char *bogohomedir);
int	clear_lock(void);
int	clear_lockfile(const char *bogohomedir);
int	needs_recovery(const char *bogohomedir);

#endif /* DB_LOCK_H */
