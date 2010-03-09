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
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>

// Prototype for compare and swap
//extern bool COMPARE_AND_SWAP(int* addr, int old_val, int new_val);

namespace mutator {
    
    pthread_cond_t all_stopped;
    int threads = 0;
 
    /** mutator::threads may be changed without a lock on collector::lock
     *  using the CAS spinlock, but to change it from zero (or less) a lock on 
     *  collector::lock must be obtained */
    inline void increment_rt_count(void);
    
    inline void decrement_rt_count(void);
    
};

/** This virtual thread exists solely to ensure that mutator::threads 
 * remains above zero when no GC is required, 
 * thus avoiding expensive locks for single threaded code 
 * entering and exiting native code. No real thread is created. */
namespace dummy_thread {
    
    int32_t running;
    
    inline void start(void) {
        running = 1;
        mutator::increment_rt_count();   
    }
#if HAVE_COMPARE_AND_SWAP
    
    inline void stop(void) {
        while (running) {
            if (COMPARE_AND_SWAP(&running, 1, 0)) {
                mutator::decrement_rt_count();
            }
        }
    }
#else
    
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    
    inline void stop(void) {
        if (running) {
            pthread_mutex_lock(&lock);
            if (running) {
                running = 0;   
                pthread_mutex_unlock(&lock);
                mutator::decrement_rt_count();
            } else {
                pthread_mutex_unlock(&lock);
            }
        }
    }
#endif
};

typedef void* (*pth_func)(void* arg);

namespace collector {
    
    pthread_t thread;
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t done;

    void run(void) {
        sigset_t   signal_mask;
        sigemptyset (&signal_mask);
        sigaddset (&signal_mask, SIGINT);
        sigaddset (&signal_mask, SIGTERM);
        pthread_sigmask (SIG_BLOCK, &signal_mask, NULL);
        dummy_thread::start();
        pthread_mutex_lock(&lock);
        do {
            // Just wait for all-stopped, do collection & repeat
            pthread_cond_wait(&mutator::all_stopped, &lock);
            if (gvmt_gc_waiting && mutator::threads == 0) {
                // Decrement threads, so debugging can see collection is occuring
                mutator::threads--;
                gvmt_do_collection();
                allocator::zero_limit_pointers();
                gvmt_gc_waiting = false;
                // "Restart" dummy thread
                dummy_thread::running = 1;
                mutator::threads = 1;
                pthread_cond_broadcast(&done);
            }
        } while (true);
    }
    
    void init(void) {
        int error = pthread_create(&thread, NULL, (pth_func)run, NULL);
        if (error) {
            fprintf(stderr, "Cannot start collector thread");
            abort();
        }
    }

}

namespace finalizer {
    
    pthread_t thread;
    
    GVMT_CALL GVMT_StackItem* finalise(GVMT_StackItem* sp, GVMT_Frame fp) {
        sigset_t   signal_mask;
        sigemptyset (&signal_mask);
        sigaddset (&signal_mask, SIGINT);
        sigaddset (&signal_mask, SIGTERM);
        pthread_sigmask (SIG_BLOCK, &signal_mask, NULL);
        // Push a few nulls to the stack for safety.
        sp -= 4;
        sp[0].p = sp[1].p = sp[2].p = sp[3].p = NULL;
        // Protect against exceptions raised in user_finalise_object.
        GvmtExceptionHandler handler = gvmt_push_handler();
        gvmt_setjump(&handler->registers);
        // Don't care if exeception was raised or not, just carry on
        do {   
            while (GC::finalization_queue.empty() || gvmt_gc_waiting) {
                mutator::wait_for_collector(sp, fp);
            }
            assert(!GC::finalization_queue.empty());
            sp[-1].o = (GVMT_Object)GC::finalization_queue.back();
            GC::finalization_queue.pop_back();
            user_finalise_object(sp-1, fp);
        } while (true);
        return 0;
    }

    void* run(void* args) {
        (void)args;
        gvmt_enter(1 << 16, (gvmt_func_ptr)finalise, 0);  
        return 0;
    }
   
    void init(void) {
        pthread_create(&thread, NULL, run, NULL);
    }
     
}

#if HAVE_COMPARE_AND_SWAP

namespace mutator {
    
    /** mutator::threads may be changed without a lock on collector::lock
     *  using the CAS spinlock, but to change it from zero a lock on 
     *  collector::lock must be obtained */
    inline void increment_rt_count(void) {
        int rt;
        do {
            rt = mutator::threads;
            // If mutator::threads < 0 it means collector is running.
            assert(rt >= 0);
            if (rt <= 0) {
                pthread_mutex_lock(&collector::lock);
                rt = mutator::threads;
                assert(rt >= 0);
                if (rt == 0) {
                    mutator::threads = 1;
                    pthread_mutex_unlock(&collector::lock);   
                    return;  
                }
                pthread_mutex_unlock(&collector::lock);    
            }
            assert(rt > 0);
        } while (!COMPARE_AND_SWAP(&mutator::threads, rt, rt+1));
    }
    
    inline void decrement_rt_count(void) {
        int rt;
        do {
            rt = mutator::threads;
            assert(rt > 0);
        } while (!COMPARE_AND_SWAP(&mutator::threads, rt, rt-1));
        if (rt == 1) {
            pthread_mutex_lock(&collector::lock);
            if (mutator::threads == 0) {
                pthread_cond_signal(&all_stopped);
            }
            pthread_mutex_unlock(&collector::lock);   
        }
    } 
    
    void wait_for_collector(GVMT_StackItem* sp, GVMT_Frame fp) {
        gvmt_stack_pointer = sp;
        gvmt_frame_pointer = fp;
        int rt;
        dummy_thread::stop();
        do {
            rt = mutator::threads;
            assert(rt > 0);
        } while (!COMPARE_AND_SWAP(&mutator::threads, rt, rt-1));
        pthread_mutex_lock(&collector::lock);
//        if (mutator::threads == 0) {
//           pthread_cond_signal(&all_stopped);
//        }
        if (gvmt_gc_waiting && mutator::threads == 0) {
            // Decrement threads, so debugging can see collection is occuring
            mutator::threads--;
            gvmt_do_collection();
            allocator::zero_limit_pointers();
            gvmt_gc_waiting = false;
            // "Restart" dummy thread
            dummy_thread::running = 1;
            mutator::threads = 1;
            pthread_cond_broadcast(&collector::done);
        } else {
            do {
                pthread_cond_wait(&collector::done, &collector::lock);
            } while (gvmt_gc_waiting);
        }
        pthread_mutex_unlock(&collector::lock);   
        increment_rt_count();
        assert(gvmt_gc_limit_pointer == 0);
        assert(gvmt_gc_free_pointer >= gvmt_gc_limit_pointer);
    }
    
}

#else

namespace mutator {
    
    /** mutator::threads may be changed only with a lock on collector::lock
     *  This is slower, but simpler than than the CAS spinlock version.
     */
    inline void increment_rt_count(void) {
        pthread_mutex_lock(&collector::lock);
        mutator::threads++;
        pthread_mutex_unlock(&collector::lock);   
    }
    
    inline void decrement_rt_count(void) {        
        pthread_mutex_lock(&collector::lock);
        mutator::threads--;
        if (mutator::threads == 0) {
            pthread_cond_signal(&all_stopped);
        }
        pthread_mutex_unlock(&collector::lock);   
    } 
    
    void wait_for_collector(GVMT_StackItem* sp, GVMT_Frame fp) {
        gvmt_stack_pointer = sp;
        gvmt_frame_pointer = fp;
        dummy_thread::stop();
        pthread_mutex_lock(&collector::lock);
        mutator::threads--;
        if (mutator::threads == 0) {
            pthread_cond_signal(&all_stopped);
        }
        do {
            pthread_cond_wait(&collector::done, &collector::lock);
        } while (gvmt_gc_waiting);
        mutator::threads++;
        pthread_mutex_unlock(&collector::lock);   
        assert(gvmt_gc_limit_pointer == 0);
        assert(gvmt_gc_free_pointer >= gvmt_gc_limit_pointer);
    }
    
}

#endif

extern "C" {

    /** Functions to assist thread-stupid debuggers ;) */
    void* __free_pointer(void) {
        return (void*)gvmt_gc_free_pointer;
    }

    void* __limit_pointer(void) {
        return (void*)gvmt_gc_limit_pointer;
    }

}
/** GC for multi-threaded application.
 * Multi-threaded, simple bump-pointer allocator, 
 * with single threaded collector
 * GC 'stops-the-world' before doing collection.
 */

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

namespace allocator {

    std::vector<GVMT_StackItem**> local_limits;
    std::vector<GVMT_StackItem**> free_pointers;
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    char* free;
    char* limit;

#if HAVE_COMPARE_AND_SWAP

    char* get_block(GVMT_StackItem* sp, GVMT_Frame fp, int size) {
        char* block;
        char* new_free;
        do {
            block = free;
            new_free = block + size;
            while (new_free > limit) {
                gvmt_gc_waiting = true;
                mutator::wait_for_collector(sp, fp);
                block = free;
                new_free = block + size;
            }
        } while (!COMPARE_AND_SWAP(&free, block, new_free));
        return block;
    }
    
#else

    char* get_block(GVMT_StackItem* sp, GVMT_Frame fp, int size) {
        char* block;
        if (gvmt_gc_waiting)
            mutator::wait_for_collector(sp, fp);
        pthread_mutex_lock(&lock);
        if (free + size > limit) {
            // Need to stop all threads.
            gvmt_gc_waiting = true;
            pthread_mutex_unlock(&lock);
            mutator::wait_for_collector(sp, fp);
            pthread_mutex_lock(&lock);
        }
        assert(free + size < limit);
        block = free;
        free += size;
        pthread_mutex_unlock(&lock);
        return block;
    }
    
#endif
    
}

namespace TLS {

    GVMT_THREAD_LOCAL GVMT_Object *array;
    std::vector<GVMT_Object**> arrays;
    uintptr_t array_size = 8;
    uintptr_t count = 0;

    uintptr_t add() {
        int result;
        pthread_mutex_lock(&collector::lock);
        if (count >= array_size) {
            uintptr_t old_size = count;
            array_size *= 2;
            for (std::vector<GVMT_Object**>::iterator it = arrays.begin(); it != arrays.end(); it++) {
                GVMT_Object* old_array = **it;
                GVMT_Object* new_array = (GVMT_Object*)malloc(sizeof(GVMT_Object) * array_size);
                memcpy(new_array, old_array, sizeof(GVMT_Object*) * old_size);
                **it = new_array;
                free(old_array);
            }
        }
        count += 1;
        result = count;
        pthread_mutex_unlock(&collector::lock);      
        return result;
    }
    
};

/** GVMT GC interface */
extern "C" {
    
    // Specific implementations need to implement:
    // void gvmt_malloc_init(uint32_t s, float residency);
    // char* gvmt_gc_name;
    
    int gvmt_gc_waiting;
    
    GVMT_Object gvmt_threads_malloc(GVMT_StackItem* sp, GVMT_Frame fp, uintptr_t size) {
        return (GVMT_Object)allocator::malloc(sp, fp, size);
    }
    
    void gvmt_gc_safe_point(GVMT_StackItem* sp, GVMT_Frame fp) {
        if (gvmt_gc_waiting) {
            mutator::wait_for_collector(sp, fp);
        }
    }
        
    GVMT_StackItem* gvmt_exit_native(void) {
        gvmt_thread_non_native = 1;
        mutator::increment_rt_count();
        return gvmt_stack_pointer;
    }
     
    GVMT_CALL void gvmt_enter_native(GVMT_StackItem* sp, GVMT_Frame fp) {
        gvmt_thread_non_native = 0;
        gvmt_stack_pointer = sp;
        gvmt_frame_pointer = fp;
        mutator::decrement_rt_count();
    }
    
    /** GVMT GC interface */
    void *gvmt_gc_add_root(void) {
        pthread_mutex_lock(&collector::lock);
        GVMT_Object* root = Root::GC_ROOTS.addRoot(NULL);
        pthread_mutex_unlock(&collector::lock);
        return root;
    }

    GVMT_LINKAGE_1(gvmt_gc_free_root, void* ref) 
        pthread_mutex_lock(&collector::lock);
        Root::GC_ROOTS.free((GVMT_Object*)ref);
        pthread_mutex_unlock(&collector::lock);
        GVMT_RETURN_V;
    }

    GVMT_LINKAGE_1(gvmt_gc_finalizable, void* obj)
        pthread_mutex_lock(&collector::lock);
        GC::finalizers.addRoot((GVMT_Object)obj);
        pthread_mutex_unlock(&collector::lock);
        GVMT_RETURN_V;
    }
    
    void* gvmt_gc_weak_reference(void) {
        pthread_mutex_lock(&collector::lock);
        void* ref = (void*)GC::weak_references.addRoot(NULL);
        pthread_mutex_unlock(&collector::lock);
        return ref;
    }

    GVMT_LINKAGE_1(gvmt_free_weak_reference, void* ref) 
        pthread_mutex_lock(&collector::lock);
        GC::weak_references.free((GVMT_Object*)ref);
        pthread_mutex_unlock(&collector::lock);
        GVMT_RETURN_V;
    }

    void inform_gc_new_stack(void) {
        pthread_mutex_lock(&collector::lock);
        GC::stacks.push_back(Stack(gvmt_stack_base, &gvmt_stack_pointer));
        GC::frames.push_back(FrameStack(&gvmt_frame_pointer));
        TLS::arrays.push_back(&TLS::array);
        TLS::array = (GVMT_Object*)malloc(sizeof(GVMT_Object*) * TLS::array_size);
        allocator::local_limits.push_back(&gvmt_gc_limit_pointer);
        allocator::free_pointers.push_back(&gvmt_gc_free_pointer);
        pthread_mutex_unlock(&collector::lock);
        gvmt_gc_free_pointer = gvmt_gc_limit_pointer = 0;
    }
    
    void* gvmt_gc_add_tls_root(void) {
        return (void*)TLS::add();
    }
    
    GVMT_LINKAGE_1(gvmt_gc_read_tls_root, void* root)
        GVMT_RETURN_R(TLS::array[(intptr_t)root]);
    }
   
    GVMT_LINKAGE_2(gvmt_gc_write_tls_root, void* root, void* obj)
        TLS::array[(intptr_t)root] = (GVMT_Object)obj;
        GVMT_RETURN_V;
    }

}

