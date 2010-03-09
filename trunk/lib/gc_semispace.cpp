#include "gvmt/internal/gc_semispace.hpp"

namespace semispace {
    
    char* to_space;
    char* from_space;
    char* top_of_space;
    char* free;
    GVMT_Object scan_ptr;
    uint32_t space_size;
    
    void init(char* heap, int heap_size) {
        space_size = heap_size/2;
        to_space = heap;
        from_space = heap + space_size;
        top_of_space = from_space;
        free = to_space;
        scan_ptr = reinterpret_cast<GVMT_Object>(free);
        GC::finalizers.intialise();
        GC::weak_references.intialise();        
    }
    
};
