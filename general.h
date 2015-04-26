
#ifndef __TCPDelegate__general__
#define __TCPDelegate__general__

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

extern void *del_alloc(size_t size);
extern void *del_allocz(size_t size);
extern void *del_realloc(void *p, size_t size);

#endif /* defined(__TCPDelegate__general__) */
