#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "wordhash.h"

#include "common.h"
#include "system.h"
#include "collect.h"

const char *spam_header_name = "X-Bogosity:"; /* unused */
int passthrough = 0; /* unused */

int max_repeats = 1;

static void print_wordlist (wordhash_t *h)
{
    hashnode_t *n;

    for(n=wordhash_first(h);n;n = wordhash_next(h)) {
	printf("%ld %s\n", ((wordprop_t *)(n->buf))->freq, n->key);
    }
}

int main(int argc, char **argv) {
    wordhash_t *h;
    long count;
    bool b;

    if (argc >= 2) max_repeats=atoi(argv[1]);
    
    do {
	collect_words(&h, &count, &b);
	printf("%ld tokens:\n", count);
	print_wordlist(h);
	printf("\n");
	wordhash_free(h);
    } while(b);
    return 0;
}
