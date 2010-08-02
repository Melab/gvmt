/** Cheney's copying collector 
 * Single threaded semi-space collector.
 */

#include <inttypes.h>
#include <stdlib.h>
#include "gvmt/internal/gc.hpp"
#include "gvmt/internal/core.h"
#include <stdio.h>
#include <string.h>

char msg1[] = "Insufficient initial memory, require at least %d bytes\n";
char msg2[] = "Cannot map initial memory block (%d bytes)\n";

namespace cheney {
        
    char *to_space, *from_space, *top_of_space, *free;
    uint32_t space_size;
    
    void init(uint32_t size) {
        uint32_t movable_size = &gvmt_end_heap-&gvmt_start_heap;
        space_size = size >> 1;
        if (space_size <= movable_size) {
            __gvmt_fatal(msg1, 2 *movable_size);
        }
        to_space = (char*)calloc(size, 1);
        if (to_space == 0)
            __gvmt_fatal(msg2, size);
        free = to_space + movable_size;
        from_space = to_space + space_size;
        top_of_space = from_space;
        GC::weak_references.intialise();
    }
    
    inline int forwarded(GVMT_Object p) { return Address(p).read_word() & FORWARDING_BIT; }
    
    inline GVMT_Object forwarding_address(GVMT_Object p) {
        Address r  = Address(reinterpret_cast<char*>(Address(p).read_word() & (~FORWARDING_BIT)));
        return r.as_object();
    }    
    
    inline void set_forwarding_address(GVMT_Object p, Address f) {
        Address(p).write_word(f.bits() | FORWARDING_BIT); 
    }
    
    /** Moves from -> to. 
     * Both from and to are real (untagged) addresses.
     * Returns the size of the object just moved */
    inline size_t move(GVMT_Object from, Address to) {
        int shape_buffer[16];
        int* shape = gvmt_shape((GVMT_Object)from, shape_buffer);
        int len =  0;
        Address real_from = Address(from);
        while (*shape) {
            if (*shape < 0)
                len -= *shape++;
            else 
                len += *shape++;
        }
        size_t size = len * sizeof(void*);
        // Copy object
        
        memcpy (reinterpret_cast<void*>(to.bits()), 
                reinterpret_cast<void*>(real_from.bits()), size);
        return size;
    }
    
    inline bool to_be_copied(GVMT_Object p) {
        if (p == NULL) {
            return false;
        }
        return true;        
    }
   
    inline GVMT_Object copy(GVMT_Object p) {
//        if (p == NULL) {
//            return p;
//        }
//        if (opaque(p)) {
//            return p;
//        }
        if (forwarded(p)) {
            return forwarding_address(p);
        }
        Address addr = Address(free);
        size_t size = align(move(p, addr));
        free = free + size;
        set_forwarding_address(p, addr);
        return addr.as_object();
    }

    void flip(void) {
        gvmt_major_collections++;
        Address scan;
//        fprintf(stderr, "Starting GC\n");
        { // Swap spaces
            char* temp = from_space; 
            from_space = to_space; 
            to_space = temp;
        }
        free = to_space; 
        top_of_space = to_space + space_size;
        scan = Address(free);
        // Copy all roots (in roots section, permanent sections and stack(s))
        for (Root::List::iterator it = Root::GC_ROOTS.begin(), 
                                  end = Root::GC_ROOTS.end(); it != end; ++it) {
            if (to_be_copied(*it))
                *it = copy(*it);
        }
        for (std::vector<FrameStack>::iterator it = GC::frames.begin(),
                          end = GC::frames.end(); it != end; ++it) {
            for (FrameStack::iterator it2 = it->begin(), 
                                  end2 = it->end(); it2 != end2; ++it2) {
                if (to_be_copied(*it2))
                    *it2 = copy(*it2);
            }
        } 
        for (std::vector<Stack>::iterator it = GC::stacks.begin(),
                          end = GC::stacks.end(); it != end; ++it) {
            for (Stack::iterator it2 = it->begin(),
                                 end2 = it->end(); it2 != end2; ++it2) {
                if (to_be_copied(*it2))
                    *it2 = copy(*it2);
            }
        }
        for (std::deque<GVMT_Object>::iterator it = GC::finalization_queue.begin(), 
                        end = GC::finalization_queue.end(); it != end; ++it) {
            if (to_be_copied(*it))
                *it = copy(*it);   
        }
        while (scan < Address(free)) {
            int shape_buffer[GVMT_MAX_SHAPE_SIZE];
            object::iterator it = object::begin(scan, shape_buffer);
            for (; !it.at_end(); ++it) {
                if (to_be_copied(*it))
                    *it = copy(*it);
            }
            scan = it.end_address();
        } 
        // Now have to scan finalizers:
        int n = GC::finalizables.size();
        for(int i = 0; i < n; i++) {
            GVMT_Object obj = GC::finalizables.back();
            GC::finalizables.pop_back();
            if (to_be_copied(obj)) {
                if (forwarded(obj)) {
                    obj = forwarding_address(obj);
                    GC::finalizables.push_front(obj);
                } else {
                    // obj is in old_space, so it needs to be finalized
                    // Copy, add to finalization Q, then remove from finalizers.
                    obj = copy(obj);
                    GC::finalization_queue.push_front(obj);
                }
            } else {
                GC::finalizables.push_front(obj);  
            }
        }
        // And scan any newly moved objects.
        while (scan < Address(free)) {
            int shape_buffer[GVMT_MAX_SHAPE_SIZE];
            object::iterator it = object::begin(scan, shape_buffer);
            for (; !it.at_end(); ++it) {
                if (to_be_copied(*it))
                    *it = copy(*it);
            }
            scan = it.end_address();
        }         
        // Scan weak refs.
        for (Root::List::iterator it = GC::weak_references.begin(), 
                           end = GC::weak_references.end(); it != end; ++it) {
            // If *it is in old_space then zero it.
            if (to_be_copied(*it)) {  
                if (forwarded(*it)) {
                    *it = forwarding_address(*it);
                } else {
                    *it = 0;
                }
            }
        }
        // Zero out remaining free memory
        memset(free, 0, top_of_space-free);
//        fprintf(stderr, "GC completed\n");
    }
    
};

