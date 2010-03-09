#include "gvmt/internal/gc.hpp"
#include "gvmt/internal/gc_generational.hpp"

#define MAX_NURSERY_SIZE (1 << 24)

namespace generational {
    
    char* xgen_pointers;
    char* starts;
    size_t size;
    char* heap;
    char* nursery;
    char* nursery_top;
    size_t nursery_size;

    void init(size_t heap_size) {
        heap_size = (heap_size + CARD_SIZE-1) & (~(CARD_SIZE-1));
        size = heap_size; 
        nursery_size = heap_size > MAX_NURSERY_SIZE * 4 ? MAX_NURSERY_SIZE : heap_size/4;
        heap = (char*)get_virtual_memory(heap_size);
        xgen_pointers = (char*)get_virtual_memory(heap_size/CARD_SIZE);
        starts = (char*)get_virtual_memory(heap_size/CARD_SIZE);
        Card base_card = card_id(heap);
        gvmt_xgen_pointer_table_offset = xgen_pointers - base_card;
        gvmt_object_start_table_offset = starts - base_card;
        nursery_top = heap + heap_size;
        nursery = nursery_top - nursery_size;
    }   
          
    void clear_cards(char* begin, char*end) {
        Card bc = card_id(begin);
        Card ec = card_id(end-1)+1;
        memset(gvmt_xgen_pointer_table_offset + bc, 0, ec-bc);
    }

}

extern "C" {
    char* gvmt_xgen_pointer_table_offset;
    char* gvmt_object_start_table_offset; 
    
    GVMT_CALL GVMT_Object gvmt_fast_allocate(size_t size) {
        size_t asize = align(size + sizeof(void*));
        char* next = ((char*)gvmt_gc_free_pointer);
        if (next + asize < (char*)gvmt_gc_limit_pointer) {
            gvmt_gc_free_pointer = (GVMT_StackItem*)(((char*)next) + asize);
            ((intptr_t*)next)[0] = 0;
            return (GVMT_Object)(next + sizeof(void*)); 
        }
        return 0;
    }
 
    
}

