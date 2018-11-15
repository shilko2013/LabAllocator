#ifndef _MEM_H_
#define _MEM_H_

#define __USE_MISC

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <sys/mman.h>

#define HEAP_START ((void*)0x04040000)

#define PAGE_SIZE 0x1000

struct mem_t;

#pragma pack(push, 1)
struct mem_t {
    struct mem_t *next;
    size_t capacity;
    int is_free;
};
#pragma pack(pop)

typedef struct mem_t;

void *_malloc(size_t query);

void _free(void *mem);

void *heap_init(size_t initial_size);

#define DEBUG_FIRST_BYTES 4

void memalloc_debug_struct_info(FILE *f,
                                struct mem_t const *const address);

void memalloc_debug_heap(FILE *f, struct mem_t const *ptr);

#endif
