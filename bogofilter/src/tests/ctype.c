/** \file ctype.c
 * This file tests if isspace(0xa0) is conformant and returns false, in
 * which case we exit with EXIT_SUCCESS (0);
 * on broken systems such as OpenBSD 2.9 to 3.6 that return true for
 * isspace(0xa0), exit with EXIT_FAILURE.
 * \author Matthias Andree
 * \date 2005
 *
 * GNU General Public License v2, without "any later version" option.
 */

#include <ctype.h>
#include <stdlib.h>

int main(void) {
    if (isspace(0xa0))
	exit(EXIT_FAILURE);
    exit(EXIT_SUCCESS);
}
