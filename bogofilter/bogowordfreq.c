/* bogowordfreq.c -- program to count token frequency of stdin */
/* (C) 2002 by Matthias Andree
 * redistributable under the terms of the GNU General Public License v2
 */

/*
 * This program parses the input and counts the frequency of the tokens
 * found, and prints a total token count and the CAPPED token frequency.
 * The maximum frequency defaults to 1 to match the recent change to the
 * Robinson algorithm. It can be overriden by giving a positive integer
 * argument on the command line.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "wordhash.h"

#include "common.h"
#include "system.h"
#include "collect.h"

const char *progname = "bogowordfreq.c";
const char *spam_header_name = "X-Bogosity:"; /* unused */
bool logflag;		/* unused */
int passthrough = 0;	/* unused */
bool quiet = 0;		/* unused */
int verbose = 0;	/* unused */
bool replace_nonascii_characters = 0;	/* unused */

static void print_wordlist (wordhash_t *h)
{
    hashnode_t *n;

    for(n=wordhash_first(h);n;n = wordhash_next(h)) {
	printf("%d %s\n", ((wordprop_t *)(n->buf))->freq, n->key);
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
