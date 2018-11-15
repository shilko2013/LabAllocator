#include "mem.h"

void memalloc_debug_struct_info(FILE *f,
                                struct mem_t const *const address) {
    size_t i;
    fprintf(f,
            "start: %p\nsize: %lu\nis_free: %d\n",
            (void *) address,
            address->capacity,
            address->is_free);
    for (i = 0;
         i < DEBUG_FIRST_BYTES && i < address->capacity;
         ++i)
        fprintf(f, "%hhX",
                ((char *) address)[sizeof(struct mem_t) + i]);
    putc('\n', f);
}

void memalloc_debug_heap(FILE *f, struct mem_t const *ptr) {
    for (; ptr; ptr = ptr->next)
        memalloc_debug_struct_info(f, ptr);
}

static uint64_t get_min_block_size(uint64_t size) {
    return size % PAGE_SIZE ? size + PAGE_SIZE - (size % 4096) : PAGE_SIZE;
}

static void init_mem_header(struct mem_t *this, struct mem_t *next, size_t capacity, int is_free) {
    if(this->next)
        this->next = next;
    this->capacity = capacity;
    this->is_free = is_free;
}

void *heap_init(size_t initial_size) {
    void *ptr = mmap(HEAP_START,
                     get_min_block_size(initial_size),
                     PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS,
                     -1, 0);
    if (ptr == MAP_FAILED)
        return MAP_FAILED;
    struct mem_t *mem_header = HEAP_START;
    init_mem_header(mem_header,
                    NULL,
                    get_min_block_size(initial_size),
                    1);
    return ptr + sizeof(struct mem_t);
}

static void *allocate_page(struct mem_t *mem_header, size_t query) {
    if (mem_header->capacity / 2 > sizeof(struct mem_t)) {
        struct mem_t *new_mem_header = mem_header + sizeof(struct mem_t) + mem_header->capacity / 2;
        init_mem_header(new_mem_header,
                        mem_header->next,
                        mem_header->capacity / 2 - sizeof(struct mem_t),
                        1);
        mem_header->next = new_mem_header;
        mem_header->capacity = mem_header->capacity / 2;
    }
    mem_header->is_free = 0;
    return mem_header + sizeof(struct mem_t);
}

static void *try_allocate_block(size_t query) {
    struct mem_t *mem_header = HEAP_START;
    do {
        if (mem_header->is_free && query + sizeof(struct mem_t) > mem_header->capacity)
            continue;
        else
            return allocate_page(mem_header, query);
    } while ((mem_header = mem_header->next) != NULL);
    return NULL;
}

static void *try_allocate_new_block(size_t query) {
    struct mem_t *mem_header = HEAP_START;
    while ((mem_header = mem_header->next) != NULL);
    void *new_block = mmap(mem_header + sizeof(struct mem_t) + mem_header->capacity,
                           get_min_block_size(query),
                           PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS,
                           -1, 0);
    if (new_block == MAP_FAILED) {
        new_block = mmap(NULL,
                         get_min_block_size(query),
                         PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS,
                         -1, 0);
        if (new_block != MAP_FAILED) {
            struct mem_t *new_mem_header = new_block;
            init_mem_header(new_mem_header,
                            NULL,
                            get_min_block_size(query),
                            1);
            mem_header->next = new_mem_header;
            return allocate_page(new_mem_header, query);
        } else
            return NULL;
    } else {
        mem_header->capacity += get_min_block_size(query);
        return allocate_page(mem_header, query);
    }
}

void *_malloc(size_t query) {
    void *result;
    if ((result = try_allocate_block(query)) != NULL)
        return result;
    else
        return try_allocate_new_block(query);
}

void *find_prev_block(struct mem_t *mem_header) {
    struct mem_t *start_mem_header = HEAP_START;
    for (; start_mem_header->next != mem_header &&
           start_mem_header->next != NULL; start_mem_header = start_mem_header->next);
    if (start_mem_header->next != mem_header)
        return NULL;
    return start_mem_header;
}

void _free(void *mem) {
    struct mem_t *mem_header = mem - sizeof(struct mem_t);
    struct mem_t *prev_mem_header = find_prev_block(mem_header);
    mem_header->is_free = 1;
    if (prev_mem_header
        && prev_mem_header->is_free
        && prev_mem_header + prev_mem_header->capacity + sizeof(struct mem_t) == mem_header) {
        prev_mem_header->capacity += mem_header->capacity + sizeof(struct mem_t);
        prev_mem_header->next = mem_header->next;
        mem_header = prev_mem_header;
    }
    if (mem_header->next
        && mem_header->next->is_free
        && mem_header->next == mem_header + mem_header->capacity + sizeof(mem_header)) {
        mem_header->capacity += mem_header->next->capacity + sizeof(struct mem_t);
        mem_header->next = mem_header->next->next;
    }
}