#ifndef GVMT_GC_THREADS_H
#define GVMT_GC_THREADS_H

/** GC for multi-threaded application.
 * Multi-threaded,simple bump-pointer allocator, 
 * with single threaded cheney's copying collector
 * GC 'stops-the-world' before doing collection.
 * and a simple bump-pointer allocator */

#include <inttypes.h>
#include <stdlib.h>
#include "gvmt/internal/gc.hpp"
#include "gvmt/internal/core.h"
#include "gvmt/native.h"
#include "gvmt/native.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>

namespace mutator {

    void wait_for_collector(GVMT_StackItem* sp, GVMT_Frame fp);
   
};

#define GVMT_ALLOCATOR_BLOCK_SIZE (1<<14)
#define GVMT_ALLOCATOR_PAGE_SIZE (1<<12)
    
namespace allocator {

    extern std::vector<GVMT_StackItem**> local_limits;
    extern std::vector<GVMT_StackItem**> free_pointers;
    extern pthread_mutex_t lock;
    extern char* free;
    extern char* limit;
    
    char* get_block(GVMT_StackItem* sp, GVMT_Frame fp, int size);    

    inline void* bump_pointer_alloc(int asize) {
        char* next = ((char*)gvmt_gc_free_pointer);
        gvmt_gc_free_pointer = (GVMT_StackItem*)(((char*)next) + asize);
        return next; 
    }
    
    inline void* malloc(GVMT_StackItem* sp, GVMT_Frame fp, size_t size) {
        // Put test here as currently compiler cannot inline fast malloc path.
        int asize = align(size);
        if (((char*)gvmt_gc_free_pointer) + asize < (char*)gvmt_gc_limit_pointer) {
            return bump_pointer_alloc(asize);
        }
        // Run out of thread memory.
        if (gvmt_gc_waiting) {
            mutator::wait_for_collector(sp, fp);
        }
        int block_size = (asize+GVMT_ALLOCATOR_BLOCK_SIZE-1) & 
                         (~(GVMT_ALLOCATOR_PAGE_SIZE-1));
        char* mem_block = get_block(sp, fp, block_size);
        gvmt_gc_free_pointer = (GVMT_StackItem*)mem_block;
        gvmt_gc_limit_pointer = (GVMT_StackItem*)(mem_block + block_size);
        return bump_pointer_alloc(asize);
    }
    
    template<class Allocator> inline char* get_block_synchronised() {
        char *result;
        if (gvmt_gc_waiting)
            return NULL;
        pthread_mutex_lock(&lock);
        result = Allocator::get_block();
        pthread_mutex_unlock(&lock);
        return result;
    }
    
    inline void zero_limit_pointers() {
        // Set all local limits and free-pointers to zero, 
        // forcing allocators to get new blocks.
        std::vector<GVMT_StackItem**>::iterator it;
        for (it = local_limits.begin(); it != local_limits.end(); it++) {
            **it = 0;
        }
        for (it = free_pointers.begin(); it != free_pointers.end(); it++) {
            **it = 0;
        }
    }
    
};

namespace finalizer {
    void init(void);
};

namespace collector {  
    void init(void); 
};  

namespace TLS {    
    uintptr_t add();
    extern GVMT_THREAD_LOCAL GVMT_Object *array;
    extern uintptr_t array_size;
};

#endif // GVMT_GC_THREADS_H

