#ifndef GVMT_ARENA_H
#define GVMT_ARENA_H
#include <stddef.h>

typedef struct arena_block* GVMT_Arena;

GVMT_Arena gvmt_new_arena(void); 

void *gvmt_allocate(size_t n, GVMT_Arena a);

void gvmt_deallocate(GVMT_Arena a);

#endif
