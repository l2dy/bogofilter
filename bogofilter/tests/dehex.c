#include <stdlib.h>
#include <stdio.h>

int main() {
    int a;
    while(scanf("%2x ", &a) == 1) {
	putchar(a);
    }
    exit(0);
}
