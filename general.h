
#ifndef __TCPDelegate__general__
#define __TCPDelegate__general__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <pthread.h>

#include "log.h"

typedef struct buf_s buf_s;

struct buf_s {
    size_t size;
    size_t bsize;
    char *data;
};

extern void buf_init(buf_s *b);
extern void buf_addc(buf_s *b, int c);
extern void buf_addunsigned(buf_s *b, unsigned long u);
extern void buf_addlong(buf_s *b, long l);
extern void buf_addddouble(buf_s *b, double d);
extern void buf_destroy(buf_s *b);

extern void *del_alloc(size_t size);
extern void *del_allocz(size_t size);
extern void *del_realloc(void *p, size_t size);

#endif /* defined(__TCPDelegate__general__) */
