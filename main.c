#include <stdio.h>
#include "mem.h"

int main() {
    heap_init(PAGE_SIZE * 2);
    int *a = _malloc(sizeof(int *));
    *a = 5;
    printf("%d", *a);
    _free(a);
    return 0;
}