#include "config.h"
#include "system.h"

#include "rand_sleep.h"

#include <stdlib.h>

void rand_sleep(double min, double max)
{
    static bool need_init = true;
    long delay;

    if (need_init) {
	struct timeval timeval;
	need_init = false;
	gettimeofday(&timeval, NULL);
	srand48(timeval.tv_usec ^ timeval.tv_sec);
    }
    delay = (int)(min + ((max-min)*drand48()));
    bf_sleep(delay);
}
