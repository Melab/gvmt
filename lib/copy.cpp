/** Single threaded GC using cheney's copying collector
 * and a simple bump-pointer allocator */

#include <inttypes.h>
#include <stdlib.h>
#include "gvmt/internal/gc.hpp"
#include "gvmt/internal/cheney.hpp"
#include "gvmt/internal/core.h"
#include <stdio.h>
#include <string.h>

char copy_name[] = "copy";

/** gvmt_gc_limit_pointer is set to <= free if finalisation is pending,
 * (gvmt_gc_limit_pointer == top_of_space otherwise) in order to speed malloc */

namespace copy {
    
    int in_malloc = 0;
    
    void finalise(GVMT_StackItem* sp, GVMT_Frame fp, GVMT_Object obj) {
        GvmtExceptionHandler handler = gvmt_create_and_push_handler(); 
        gvmt_double_return_t val;
        val.ret = gvmt_setjump(&handler->registers, sp);
        void *ex = val.regs.ex;
        if (ex == 0) {
            sp--;
            sp->o = obj;
            user_finalise_object(sp, fp);               
        }
        gvmt_pop_and_free_handler();
    }
    
    inline void* malloc(GVMT_StackItem* sp, GVMT_Frame fp, size_t size) {
        char *result = (char*)gvmt_gc_free_pointer;
        size_t asize = align(size);
        gvmt_gc_free_pointer += asize;
        if (gvmt_gc_free_pointer >= gvmt_gc_limit_pointer) {
            if (!GC::finalization_queue.empty() && in_malloc==0) {
                ++in_malloc;
                GVMT_Object obj = (GVMT_Object)GC::finalization_queue.back();
                GC::finalization_queue.pop_back();
                if (GC::finalization_queue.empty())
                    gvmt_gc_limit_pointer = (GVMT_StackItem*)cheney::top_of_space;
                finalise(sp, fp, obj);
                --in_malloc;
            }
            if (((char*)gvmt_gc_free_pointer) >= cheney::top_of_space) {
                cheney::flip();
                gvmt_gc_free_pointer = (GVMT_StackItem*)cheney::free;
                if (GC::finalization_queue.empty())
                    gvmt_gc_limit_pointer = (GVMT_StackItem*)cheney::top_of_space;
                else
                    gvmt_gc_limit_pointer = (GVMT_StackItem*)gvmt_gc_free_pointer;
                if (((char*)gvmt_gc_free_pointer) + asize >= cheney::top_of_space) {
                    user_gc_out_of_memory(); 
                }
                result = (char*)gvmt_gc_free_pointer;
                gvmt_gc_free_pointer = (GVMT_StackItem*)(result + asize);
            }
        }
        return result;
    }

}    


/** GVMT GC interface */
extern "C" {

    char* gvmt_gc_name = &copy_name[0];
    
    void gvmt_malloc_init(uint32_t s, float residency) {
        cheney::init(s);
        gvmt_gc_free_pointer = (GVMT_StackItem*)cheney::free;
        gvmt_gc_limit_pointer = (GVMT_StackItem*)cheney::top_of_space;
    }
    
    GVMT_Object gvmt_copy_malloc(GVMT_StackItem* sp, GVMT_Frame fp, uintptr_t size) {
        gvmt_stack_pointer = sp;
        gvmt_frame_pointer = fp;
        return (GVMT_Object)copy::malloc(sp, fp, size);
    }
    
    void gvmt_gc_safe_point(GVMT_StackItem* sp, GVMT_Frame fp) {
        // Don't need to do anything for single-threaded code.
    }

    GVMT_StackItem* gvmt_exit_native(void) {
        return gvmt_stack_pointer;
    }
     
    GVMT_CALL void gvmt_enter_native(GVMT_StackItem* sp, GVMT_Frame fp) {
        gvmt_stack_pointer = sp;
        gvmt_frame_pointer = fp;
    }

    GVMT_LINKAGE_1(gvmt_gc_finalizable, void* obj)
        GC::finalizables.push_back((GVMT_Object)obj);
        GVMT_RETURN_V;
    }
    
    void* gvmt_gc_weak_reference(void) {
        return (void*)GC::weak_references.addRoot(NULL);
    }
            
    GVMT_LINKAGE_1(gvmt_free_weak_reference, void* ref)
        GC::weak_references.free((GVMT_Object*)ref);
        GVMT_RETURN_V;
    }
    
    void inform_gc_new_stack(void) {
        GC::stacks.push_back(Stack(gvmt_stack_base, &gvmt_stack_pointer));
        GC::frames.push_back(FrameStack(&gvmt_frame_pointer));
        if (GC::stacks.size() != 1) {
            fprintf(stderr, "Multiple threads for single thread allocator");
            abort();
        }
    }

}


