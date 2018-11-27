#include <stdio.h>
#include "mem.h"

int main() {
    heap_init(PAGE_SIZE * 6);
    int *a;
    memalloc_debug_heap(stdout, HEAP_START);
    printf("\n");
    a = _malloc(sizeof(int *));
    memalloc_debug_heap(stdout, HEAP_START);
    printf("\n");
    a = _malloc(sizeof(int *)*60000);
    memalloc_debug_heap(stdout, HEAP_START);
    printf("\n");
}

