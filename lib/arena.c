#include "gvmt/arena.h"
#include <assert.h>
#include <malloc.h>
#include <string.h>

struct arena_block {
    struct arena_block *next;
    char *limit;
    char *avail;
};

union align {
    long l;
    char *p;
    double d;
    int (*f)(void);
};

#define roundup(x,n) (((x)+((n)-1))&(~((n)-1)))
#define max(a,b) ((a)>(b)?(a):(b))

static struct arena_block *freeblocks = 0;


/** Either returns a block from the freelist or gets a new block from the OS 
 *  The avail and limit pointers are set to their initial values. */
struct arena_block *allocate_block(unsigned size) {
    unsigned m = roundup(size, sizeof (union align));
    struct arena_block **prev = &freeblocks;
    struct arena_block *free = freeblocks;
    while(free) {
        if (free->limit - free->avail >= m) {
            *prev = free->next;
            return free;
        }
        prev = &(free->next);
        free = free->next;
    }
    int default_size = roundup(16000, sizeof (union align));
    m = max(default_size, m);
    free = malloc(m + sizeof(struct arena_block));
    if (free == 0) {
        // fatal("Out of memory");
        assert(0);   
    }
    free->avail = ((char*)free) + sizeof(struct arena_block);
    free->limit = free->avail + m;
    free->next = 0;
    assert(free->limit - free->avail >= roundup(size, sizeof (union align)));
    return free;
}

GVMT_Arena gvmt_new_arena(void) {
    GVMT_Arena a = allocate_block(0);
    a->next = a;
    return a;
}

void *gvmt_allocate(size_t n, GVMT_Arena a) {
    struct arena_block *free = a->next;
//    printf("Allocate request for %u\n", n);
    n = roundup(n, sizeof (union align));
    if (n > free->limit - free->avail) {
        free = allocate_block(n);
        free->next = a->next;
        a->next = free;
    }
    char *result = free->avail;
    free->avail += n;
    assert(a->next->limit - result >= n);
    memset(result, 0, n);
    return result;
}

void gvmt_deallocate(GVMT_Arena a) {
    struct arena_block* block = a;
    do {
        struct arena_block* next = block->next;
        block->avail = ((char*)block) + sizeof(struct arena_block);
        block->next = freeblocks;
        freeblocks = block;
        block = next;
    } while (block != a);
}
