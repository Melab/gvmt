/** Null collector - Just allocate until no memory left */

#include <inttypes.h>
#include <stdlib.h>
#include <assert.h>
#include "gvmt/internal/core.h"
 
static char* next;
static char* limit;

#define MEM_SPACE (256 * 1024 * 1024)
 
static int initialised = 0;

char* gvmt_gc_name = "none";

void gvmt_malloc_init(uint32_t s, float residency) {
    initialised = 1;
    next = calloc(MEM_SPACE, 1);
    if (next == 0)
        __gvmt_fatal("Cannot map initial memory block\n");       
    limit = next + MEM_SPACE;
}

GVMT_Object gvmt_none_malloc(GVMT_StackItem* sp, GVMT_Frame fp, uintptr_t size) {
    void* new_object = next;
    assert(initialised);
    next += size;
    if (next >= limit)
        __gvmt_fatal("Out of memory");
    return (GVMT_Object)new_object;
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
    
GVMT_LINKAGE_1(gvmt_gc_finalizable, GVMT_Object obj)
    (void)obj;
    GVMT_RETURN_V;
}
    
void *gvmt_gc_add_root(void) {
    GVMT_Object* root = (GVMT_Object*)malloc(sizeof(void*));
    *root = NULL;
    return (void*)root;
}
    
GVMT_LINKAGE_1(gvmt_gc_free_root, GVMT_Object* ref)
    free(ref);    
    GVMT_RETURN_V;
}

void *gvmt_gc_weak_reference() {
    GVMT_Object* root = (GVMT_Object*)malloc(sizeof(void*));
    *root = NULL;
    return (void*)root;
}

GVMT_LINKAGE_1(gvmt_gc_free_weak_reference, GVMT_Object* ref)
    free(ref);
    GVMT_RETURN_V;
}

void inform_gc_new_stack(void) {
    // Don't need to do anything for single-threaded code.
}

/** Same as single threaded versions */
void* gvmt_gc_add_tls_root(void) {
    void** root = (void**)malloc(sizeof(void*));
    *root = 0;
    return (void*)root;
}

GVMT_LINKAGE_1(gvmt_gc_read_tls_root, void* root)
    GVMT_RETURN_R(*((GVMT_Object*)root));
}

GVMT_LINKAGE_2(gvmt_gc_write_tls_root, void* root, void* obj)
    *((GVMT_Object*)root) = (GVMT_Object)obj;
    GVMT_RETURN_V;
}

GVMT_LINKAGE_1 (gvmt_gc_pin, void* obj)
    GVMT_RETURN_P(obj);
}


