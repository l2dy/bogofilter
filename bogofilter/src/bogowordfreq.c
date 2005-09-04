/* $Id$ */

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

#include "bogoconfig.h"
#include "bogoreader.h"
#include "charset.h"
#include "collect.h"
#include "mime.h"
#include "wordhash.h"
#include "wordlists.h"

const char *progname = "bogowordfreq.c";
const char *spam_header_name = "X-Bogosity:"; /* unused */

/* function definitions */

static void initialize(void)
{
    init_charset_table(charset_default);
    mime_reset();
    token_init();
    lexer_v3_init(NULL);
}

static void print_wordlist (wordhash_t *h)
{
    hashnode_t *n;

    for (n=wordhash_first(h);n;n = wordhash_next(h)) {
      word_t *key = n->key;
      (void) printf ("%d ", ((wordprop_t *)(n->buf))->freq);
      (void) fwrite(key->text, 1, key->leng, stdout);
      (void) fputc('\n', stdout);
    }
}

int main(int argc, char **argv)
{
#ifdef __EMX__
    _response (&argc, &argv);	/* expand response files (@filename) */
    _wildcard (&argc, &argv);	/* expand wildcards (*.*) */
#endif

    max_repeats=1;		/* set default value */

    if (argc >= 2) {
	max_repeats=atoi(argv[1]);
	argc -= 1;
	argv += 1;
    }

    fpin = stdin;
    dbgout = stdout;

    mbox_mode = true;		/* to allow multiple messages */
    bogoreader_init(argc, argv);

    while (reader_more()) {
	wordhash_t *h = wordhash_new();
	initialize();
	collect_words(h);
	printf("%ld tokens:\n", (long) h->count);
	print_wordlist(h);
	printf("\n");
	wordhash_free(h);
    }
    return 0;
}

