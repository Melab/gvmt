#include "gvmt/internal/gc.hpp"
#include "gvmt/internal/SemiSpace.hpp"
#include "gvmt/internal/Generational.hpp"
 
typedef Generational<SemiSpace> GenCopy;

void gvmt_do_collection() {
    GenCopy::collect();
}

static char gencopy2_name[] = "gencopy2";

extern "C" {

    char* gvmt_gc_name = &gencopy2_name[0];
   
    GVMT_Object gvmt_gencopy2_malloc(GVMT_StackItem* sp, GVMT_Frame fp, size_t size) {
        return GenCopy::allocate(sp, fp, size);
    }

    GVMT_CALL GVMT_Object gvmt_fast_allocate(size_t size) {
        return GenCopy::fast_allocate(size);
    }

    void gvmt_malloc_init(size_t heap_size_hint) {
        GenCopy::init(heap_size_hint);
        Zone::verify_heap();
        LargeObjectSpace::verify_heap();
    }
        
    GVMT_CALL void* gvmt_gc_pin(GVMT_Object obj) {
        return GenCopy::pin(obj);
    }
    
    int gvmt_is_pinned(void* ptr) {
        return GenCopy::is_pinned(ptr);
    }
    
}



