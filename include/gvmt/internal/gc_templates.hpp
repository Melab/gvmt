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
        for (Root::List::iterator it = GC::finalizers.begin(), 
                        end = GC::finalizers.end(); it != end; ++it) {
            if (Collection::wants(*it)) {
                if (Collection::is_live(*it)) {
                    *it = Collection::apply(*it);
                } else {
                    // *it is not live, so it needs to be finalized
                    // Keep live, add to finalization Q, then remove from finalizers.
                    *it = Collection::apply(*it);
                    GC::finalization_queue.push_front(*it);
                    GC::finalizers.free(it);
                }
            }
        }
    }    
    
    template <class Collection> inline void process_weak_refs() {
        for (Root::List::iterator it = GC::weak_references.begin(), 
                           end = GC::weak_references.end(); it != end; ++it) {
            // If *it is not live then zero it.
            if (Collection::wants(*it)) {  
                if (Collection::is_live(*it)) {
                    *it = Collection::apply(*it);
                } else {
                    *it = 0;
                }
            }
        }
    }
    
    template <class Collection> inline Address scan_object(Address addr) {
        int shape_buffer[GVMT_MAX_SHAPE_SIZE];
        object::iterator it = object::begin(addr, shape_buffer);
        for (; !it.at_end(); ++it) {
            if (Collection::wants(*it))
                *it = Collection::apply(*it);
        }
        return it.end();
    }
    
    template <class Collection> void transitive_closure() {
        while (!GC::mark_stack.empty()) {
            Address obj = GC::mark_stack.back();
            GC::mark_stack.pop_back();
            Address end = scan_object<Collection>(obj);
            Collection::scanned(obj, end);
        }
    }
    
    template <class Collection> GVMT_Object grey(GVMT_Object obj) {
        if (Collection::wants(obj))
            return Collection::apply(obj);
    }

}

#endif // GVMT_INTERNAL_GC_TEMPLATES_H 
