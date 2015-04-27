#include "general.h"

#define MAX_NUMLEN 512

void buf_init(buf_s *b)
{
    enum {INIT_BSIZE = 128};
    
    b->size = 0;
    b->bsize = INIT_BSIZE;
    b->data = del_alloc(INIT_BSIZE);
}

void buf_addc(buf_s *b, int c)
{
    if(b->size == b->bsize) {
        b->bsize *= 2;
        b->data = del_realloc(b->data, b->bsize);
    }
    b->data[b->size++] = c;
}

void buf_addunsigned(buf_s *b, unsigned long u)
{
    char buf[MAX_NUMLEN], *ptr = buf;
    
    sprintf(buf, "%lu", u);
    
    do {
        buf_addc(b, *ptr);
    } while(*++ptr);
}

void buf_addlong(buf_s *b, long l)
{
    char buf[MAX_NUMLEN], *ptr = buf;
    
    sprintf(buf, "%ld", l);
    
    do {
        buf_addc(b, *ptr);
    } while(*++ptr);
}

void buf_addddouble(buf_s *b, double d)
{
    char buf[MAX_NUMLEN], *ptr = buf;
    
    sprintf(buf, "%f", d);
    
    do {
        buf_addc(b, *ptr);
    } while(*++ptr);
}

void buf_destroy(buf_s *b)
{
    free(b->data);
}

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