#include "gvmt/internal/gc.hpp"
#include "gvmt/internal/Immix.hpp"
#include "gvmt/internal/Generational.hpp"
 
typedef Generational<Immix> GenImmix;

void gvmt_do_collection() {
    GenImmix::collect();
}

static char genimmix2_name[] = "genimmix2";

extern "C" {

    char* gvmt_gc_name = &genimmix2_name[0];
   
    GVMT_Object gvmt_genimmix2_malloc(GVMT_StackItem* sp, GVMT_Frame fp, size_t size) {
        return GenImmix::allocate(sp, fp, size);
    }

    GVMT_CALL GVMT_Object gvmt_fast_allocate(size_t size) {
        return GenImmix::fast_allocate(size);
    }

    void gvmt_malloc_init(size_t heap_size_hint, float residency) {
        GenImmix::init(heap_size_hint, residency);
        Zone::verify_heap();
    }
    
    void gvmt_gc_collect(void) {
        GenImmix::full_collect();
    }
        
    GVMT_CALL void* gvmt_gc_pin(GVMT_Object obj) {
        assert(obj);
        return GenImmix::pin(obj);
    }
    
}

