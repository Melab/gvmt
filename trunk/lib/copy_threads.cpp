/** GC for multi-threaded application.
 * Multi-threaded,simple bump-pointer allocator, 
 * with single threaded cheney's copying collector
 * GC 'stops-the-world' before doing collection.
 * and a simple bump-pointer allocator */

/** This collector has 3 states:
 *  -- Collecting. 
 *  -- All stopped
 *  -- Not collecting
 * The transistion between these states is guarded by the mutex 
 * collector::lock
 * The global variable mutator::threads (protected with CAS spin-locks)
 * counts the number of running (in GVMT) threads.
 * A thread running native code does NOT count towards the total.
 * mutator::threads < 0 <=> Collecting
 * mutator::threads == 0 <=> All stopped
 * mutator::threads > 0 <=> Not collecting.
 * In order to maintain safety, a lock on collector::lock
 * must be obtained before mutator::threads can be altered to or from 0.
 * Additionally in order to halt threads reentering GVMT code from native,
 * the collector locks collector::lock while it is collecting.
 */

// READ THIS
// Until the llvm-JIT for x86 supports thread-local storage,
// Allocation is always via gvmt_copy_threads_malloc(), rather than inlined,

#include <inttypes.h>
#include <stdlib.h>
#include "gvmt/internal/gc.hpp"
#include "gvmt/internal/gc_threads.hpp"
#include "gvmt/internal/cheney.hpp"
#include "gvmt/internal/core.h"
#include "gvmt/native.h"
#include "gvmt/native.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>

char copy_threads_name[] = "copy_threads";

void gvmt_do_collection() {
    cheney::flip();
    allocator::free = cheney::free;
    allocator::limit = cheney::top_of_space;
}               
                
/** GVMT GC interface */
extern "C" {

    char* gvmt_gc_name = &copy_threads_name[0];
    
    void gvmt_malloc_init(uint32_t s, float residency) {
        cheney::init(s);
        allocator::free = cheney::free;
        allocator::limit = cheney::top_of_space;
        mutator::init();
        collector::init();
        finalizer::init();
    }
    
    GVMT_Object gvmt_copy_threads_malloc(GVMT_StackItem* sp, GVMT_Frame fp, uintptr_t size) {
        return (GVMT_Object)allocator::malloc(sp, fp, size);
    }

}

