#include "mem.h"

void memalloc_debug_struct_info(FILE *f, mem_t const *const address) {
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
                ((char *) address)[sizeof(mem_t) + i]);
    putc('\n', f);
}

void memalloc_debug_heap(FILE *f, mem_t const *ptr) {
    for (; ptr; ptr = ptr->next)
        memalloc_debug_struct_info(f, ptr);
}

static uint64_t get_min_block_size(uint64_t size) {
    return size % PAGE_SIZE ? size + PAGE_SIZE - (size % 4096) : size;
}

static void init_mem_header(mem_t *this, mem_t *next, size_t capacity, int is_free) {
    this->next = next;
    this->capacity = capacity;
    this->is_free = is_free;
}

static void part_memory(size_t initial_size) {
    mem_t *mem_header = HEAP_START;
    size_t number_blocks = initial_size / (BLOCK_SIZE + sizeof(mem_t));
    mem_t *prev_mem_header;
    for (size_t i = 0; i < number_blocks; ++i) {
        init_mem_header(mem_header, (mem_t *) (((char *) mem_header) + sizeof(mem_t) + BLOCK_SIZE),
                        BLOCK_SIZE, 1);
        prev_mem_header = mem_header;
        mem_header = (mem_t *) (((char *) mem_header) + sizeof(mem_t) + BLOCK_SIZE);
    }
    size_t remain = (initial_size - (BLOCK_SIZE + sizeof(mem_t)) * (number_blocks - 1));
    if (!remain)
        prev_mem_header->next = NULL;
    else if (remain <= sizeof(mem_t)) {
        prev_mem_header->next = NULL;
        prev_mem_header->capacity += remain;
    } else
        init_mem_header(mem_header, NULL, remain, 1);
}

void *heap_init(size_t initial_size) {
    void *ptr = mmap(HEAP_START,
                     get_min_block_size(initial_size),
                     PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS,
                     -1, 0);
    if (ptr == MAP_FAILED)
        return MAP_FAILED;
    mem_t *mem_header = HEAP_START;
    init_mem_header(mem_header,
                    NULL,
                    get_min_block_size(initial_size),
                    1);
    part_memory(get_min_block_size(initial_size));
    return ((char *) ptr) + sizeof(mem_t);
}

static void *allocate_page(mem_t *mem_header, size_t query) {
    if (mem_header->capacity / 2 > sizeof(mem_t)) {
        mem_t *new_mem_header = (mem_t *) (((char *) mem_header) + sizeof(mem_t) +
                                                         mem_header->capacity / 2);
        init_mem_header(new_mem_header,
                        mem_header->next,
                        mem_header->capacity / 2 - sizeof(mem_t),
                        1);
        mem_header->next = new_mem_header;
        mem_header->capacity = mem_header->capacity / 2;
    }
    mem_header->is_free = 0;
    return ((char *) mem_header) + sizeof(mem_t);
}

static void *try_allocate_block(size_t query) {
    mem_t *mem_header = HEAP_START;
    do {
        if (query + sizeof(mem_t) > mem_header->capacity)
            continue;
        else if (mem_header->is_free)
            return allocate_page(mem_header, query);
    } while ((mem_header = mem_header->next) != NULL);
    return NULL;
}

static void *try_allocate_new_block(size_t query) {
    mem_t *mem_header = HEAP_START;
    for (; mem_header->next != NULL; mem_header = mem_header->next);
    void *new_block = mmap(((char *) mem_header) + sizeof(mem_t) + mem_header->capacity,
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
            mem_t *new_mem_header = new_block;
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

void *find_prev_block(mem_t *mem_header) {
    mem_t *start_mem_header = HEAP_START;
    for (; start_mem_header->next != mem_header &&
           start_mem_header->next != NULL; start_mem_header = start_mem_header->next);
    if (start_mem_header->next != mem_header)
        return NULL;
    return start_mem_header;
}

void _free(void *mem) {
    mem_t *mem_header = mem - sizeof(mem_t);
    mem_t *prev_mem_header = find_prev_block(mem_header);
    mem_header->is_free = 1;
    if (prev_mem_header
        && prev_mem_header->is_free
        && ((mem_t *) ((char *) prev_mem_header) + prev_mem_header->capacity + sizeof(mem_t)) ==
           mem_header) {
        prev_mem_header->capacity += mem_header->capacity + sizeof(mem_t);
        prev_mem_header->next = mem_header->next;
        mem_header = prev_mem_header;
    }
    if (mem_header->next
        && mem_header->next->is_free
        && mem_header->next == ((mem_t *) ((char *) mem_header) + mem_header->capacity + sizeof(mem_header))) {
        mem_header->capacity += mem_header->next->capacity + sizeof(mem_t);
        mem_header->next = mem_header->next->next;
    }
}
