#include "config.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>

#include "globals.h"
#include "set_bogohome.h"
#include "xmalloc.h"
#include "xstrdup.h"

void set_bogohome(const char *ds_file) {
    char *t;
    struct stat st;

    bogohome = xstrdup(ds_file);
    if (lstat(bogohome, &st) != 0 || !S_ISDIR(st.st_mode))
	if ((t = strrchr(bogohome, DIRSEP_C)))
	    *t = '\0';
    if (!*bogohome) {
	free(bogohome);
	bogohome = xstrdup(CURDIR_S);
    }
}


