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

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "collect.h"
#include "wordhash.h"
#include "wordlists.h"

const char *progname = "bogowordfreq.c";
const char *spam_header_name = "X-Bogosity:"; /* unused */

wordlist_t* word_lists=NULL;	/* define to satisfy link requirements of msgcount.c */

static void print_wordlist (wordhash_t *h)
{
    hashnode_t *n;

    for(n=wordhash_first(h);n;n = wordhash_next(h)) {
      word_t *key = n->key;
      (void) printf ("%d ", ((wordprop_t *)(n->buf))->freq);
      (void) fwrite(key->text, 1, key->leng, stdout);
      (void) fputc('\n', stdout);
    }
}

int main(int argc, char **argv)
{
    bool more;

    if (argc >= 2) max_repeats=atoi(argv[1]);
    
    do {
	wordhash_t *h = wordhash_init();
	more = collect_words(h);
	printf("%ld tokens:\n", (long) h->wordcount);
	print_wordlist(h);
	printf("\n");
	wordhash_free(h);
    } while (more);
    return 0;
}
