#include "buff_fgetsl.h"
#include "fgetsl.h"
#include "buff.h"

int buff_fgetsl(buff_t *b, FILE *in)
{
    int s = xfgetsl(b->t.text, b->size, in, 1);
    if (s >= 0) {
	b->t.leng = s;
    }
    b->read = 0;
    return s;
}
