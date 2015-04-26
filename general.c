#include "general.h"

void *del_alloc(size_t size)
{
    void *p = malloc(size);
    if(!p) {
        perror("Memory Allocation Error (malloc)");
        exit(EXIT_FAILURE);
    }
    return p;
}

void *del_allocz(size_t size)
{
    void *p = calloc(size, 1);
    if(!p) {
        perror("Memory Allocation Error (calloc)");
        exit(EXIT_FAILURE);
    }
    return p;
}

void *del_realloc(void *p, size_t size)
{
    void *np = realloc(p, size);
    if(!np) {
        perror("Memory Allocation Error (realloc)");
        exit(EXIT_FAILURE);
    }
    return np;
}