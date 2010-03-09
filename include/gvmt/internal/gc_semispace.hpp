#include "gvmt/internal/gc.hpp"

namespace semispace {
    
    extern char* free;    
    extern char* to_space;
    extern char* from_space;
    extern char* top_of_space;
    extern char* free;
    extern uint32_t space_size;
    extern GVMT_Object scan_ptr;
    
    inline int forwarded(GVMT_Object p) { return gc::untag(p).read_word() & FORWARDING_BIT; }
    
    inline GVMT_Object forwarding_address(GVMT_Object p) {
        Address r  = Address::from_bits(gc::untag(p).read_word() & (~FORWARDING_BIT));
        return gc::tag(r, gc::get_tag(p));
    }    
    
    inline void set_forwarding_address(GVMT_Object p, Address f) {
        gc::untag(p).write_word(f.bits() | FORWARDING_BIT); 
    }
    
    inline GVMT_Object move(GVMT_Object from) {
        int shape_buffer[16];
        int* shape = gvmt_shape((GVMT_Object)from, shape_buffer);
        int len =  0;
        Address to = Address(free);
        Address real_from = gc::untag(from);
        while (*shape) {
            if (*shape < 0)
                len -= *shape++;
            else 
                len += *shape++;
        }
        size_t size = len * sizeof(void*);
        free = free + align(size);
        Address to_ptr, from_ptr;
        to_ptr = to;
        from_ptr = real_from;
        // Copy first word - All objects are word aligned 
        // and must use some memory, so it is always safe to copy first word
        to_ptr.write_word(from_ptr.read_word()); 
        to_ptr = to_ptr.next_word();
        if (to_ptr < Address(free)) {
            from_ptr = from_ptr.next_word();
            do {
                to_ptr.write_word(from_ptr.read_word()); 
                to_ptr = to_ptr.next_word();
                from_ptr = from_ptr.next_word();
            } while (to_ptr < Address(free));
        }
        set_forwarding_address(from, to);
        return gc::tag(to, gc::get_tag(from));
    }

    inline GVMT_Object copy(GVMT_Object p) {
        if (forwarded(p)) {
            return forwarding_address(p);
        }
        return move(p);
    }
    
    void init(char* heap, int heap_size);
    
    inline void swap_spaces() {
        char* temp = from_space; 
        from_space = to_space; 
        to_space = temp;
        free = to_space;
        top_of_space = to_space + space_size;
        scan_ptr = reinterpret_cast<GVMT_Object>(free);
    }
       
    template <class Collection> inline void scan() {
        Address local_scan = scan_ptr;
        while (local_scan < Address(free)) {
            int shape_buffer[GVMT_MAX_SHAPE_SIZE];
            object::iterator it = object::begin(local_scan, shape_buffer);
            for (; !it.at_end(); ++it) {
                if (Collection::wants(*it))
                    *it = Collection::apply(*it);
            }
            local_scan = it.end();
        } 
        scan_ptr = local_scan.as_object();
    }

}    
    
