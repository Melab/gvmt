#include "gvmt/internal/gc.hpp"
#include "gvmt/internal/NonGenerational.hpp"
#include "gvmt/internal/SemiSpace.hpp"
 
typedef NonGenerational<SemiSpace> NonGenCopy;

void gvmt_do_collection() {
    NonGenCopy::collect();
}

static char copy2_name[] = "copy2";

extern "C" {

    char* gvmt_gc_name = &copy2_name[0];
   
    GVMT_Object gvmt_copy2_malloc(GVMT_StackItem* sp, GVMT_Frame fp, size_t size) {
        return NonGenCopy::allocate(sp, fp, size);
    }

    GVMT_CALL GVMT_Object gvmt_fast_allocate(size_t size) {
        return NonGenCopy::fast_allocate(size);
    }

    void gvmt_malloc_init(size_t heap_size_hint, float residency) {
        NonGenCopy::init(heap_size_hint, residency);
        SuperBlock::verify_heap();
    }
  
    GVMT_CALL void* gvmt_gc_pin(GVMT_Object obj) {
        return NonGenCopy::pin(obj);
    }
    
}



