#include "gvmt/internal/gc.hpp"
 
#ifndef GVMT_INTERNAL_GC_TEMPLATES_H 
#define GVMT_INTERNAL_GC_TEMPLATES_H 

namespace gc {
        
    template <class Collection> inline void process_roots() {
        for (Root::List::iterator it = Root::GC_ROOTS.begin(), 
                                  end = Root::GC_ROOTS.end(); it != end; ++it) {
            if (Collection::wants(*it))
                *it = Collection::apply(*it);
        }
        for (std::vector<FrameStack>::iterator it = GC::frames.begin(),
                          end = GC::frames.end(); it != end; ++it) {
            for (FrameStack::iterator it2 = it->begin(), 
                                  end2 = it->end(); it2 != end2; ++it2) {
                if (Collection::wants(*it2))
                    *it2 = Collection::apply(*it2);
            }
        } 
        for (std::vector<Stack>::iterator it = GC::stacks.begin(),
                          end = GC::stacks.end(); it != end; ++it) {
            for (Stack::iterator it2 = it->begin(),
                                 end2 = it->end(); it2 != end2; ++it2) {
                if (Collection::wants(*it2))
                    *it2 = Collection::apply(*it2);
            }
        }
        for (std::deque<GVMT_Object>::iterator it = GC::finalization_queue.begin(), 
                        end = GC::finalization_queue.end(); it != end; ++it) {
            if (Collection::wants(*it))
                *it = Collection::apply(*it);   
        }
    }
    
    template <class Collection> inline void process_finalisers() {
        int n = GC::finalizables.size();
        for(int i = 0; i < n; i++) {
            GVMT_Object obj = GC::finalizables.back();
            GC::finalizables.pop_back();
            if (Collection::wants(obj)) {
                if (Collection::is_live(Address(obj))) {
                    obj = Collection::apply(obj);
                    GC::finalizables.push_front(obj);
                } else {
                    // obj is not live, so it needs to be finalized
                    // Keep live, add to finalization Q, then remove from finalizables.
                    obj = Collection::apply(obj);
                    GC::finalization_queue.push_front(obj);
                }
            } else {
                GC::finalizables.push_front(obj);  
            }
        }
    }    
    
    template <class Collection> inline void process_weak_refs() {
        for (Root::List::iterator it = GC::weak_references.begin(), 
                           end = GC::weak_references.end(); it != end; ++it) {
            // If *it is not live then zero it.
            if (Collection::wants(*it)) {  
                if (Collection::is_live(Address(*it))) {
                    *it = Collection::apply(*it);
                } else {
                    *it = 0;
                }
            }
        }
    }
    
//Should refactor this, to allow custom scanners for different VMs.
#ifdef HOTPY_SPECIFIC
    
    /****** HotPy specific version ***/
        
    struct var_object_head {
        struct type_head* klass;
        uintptr_t ref_count;
        uintptr_t length;
    };
    
    struct type_head {
        struct type_head* klass;
        uintptr_t ref_count;
        uint32_t flags;
        char* tp_name;
        int32_t shape[5];
        int32_t variable;
        uint32_t length;
        intptr_t dict_offset;
    };
    
    template <class Collection> inline Address scan_object(Address addr) {
        GVMT_Object* item = reinterpret_cast<GVMT_Object*>(addr.bits());
        struct var_object_head* var_obj = reinterpret_cast<var_object_head*>(item);
        GVMT_Object field = *item;
        struct type_head* klass = var_obj->klass;
        if (Collection::wants(field)) {
            *item = Collection::apply(field);
        }
        assert(klass->shape[0] == 1);
        assert(klass->shape[1] < 0);
        item += 1-klass->shape[1];
        assert(klass->shape[2] >= 0);
        GVMT_Object* end_chunk = item + klass->shape[2];
        for (; item < end_chunk; item++) {
            GVMT_Object field = *item;
            if (Collection::wants(field)) {
                *item = Collection::apply(field);
            }
        }
        assert(item == end_chunk);
        assert(klass->shape[3] <= 0);
        item -= klass->shape[3];
        assert(klass->shape[4] == 0);
        if (klass->variable) {
            if (klass->variable > 0) {
                end_chunk = item + var_obj->length;
                for (; item < end_chunk; item++) {
                    GVMT_Object field = *item;
                    if (Collection::wants(field)) {
                        *item = Collection::apply(field);
                    }
                }
            } else {
                intptr_t size = klass->variable * var_obj->length;
                size >>= (sizeof(void*)==8?3:2);
                item -= size;
            }
            if (klass->dict_offset) {
                GVMT_Object field = *item;
                if (Collection::wants(field)) {
                    *item = Collection::apply(field);
                }
                item += 2; // dict & lock.
            }
        }
        return Address(item);
    }

#else
    
    template <class Collection> inline Address scan_object(Address addr) {
        int shape_buffer[GVMT_MAX_SHAPE_SIZE];
        GVMT_Object* start = reinterpret_cast<GVMT_Object*>(addr.bits());
        GVMT_Object* item = start;
        int* shape = gvmt_user_shape(addr.as_object(), shape_buffer);
        while (*shape) {
            if (*shape < 0) {
                item -= *shape;
            } else {
                GVMT_Object* end_chunk = item + *shape;
                for (; item < end_chunk; item++) {
                    GVMT_Object field = *item;
                    if (Collection::wants(field)) {
                        *item = Collection::apply(field);
                    }
                }
            }
            ++shape;
        }
        assert(item == start + gvmt_user_length(addr.as_object())/sizeof(void*));
        return Address(item);
    }
    
#endif

}

#endif // GVMT_INTERNAL_GC_TEMPLATES_H 
