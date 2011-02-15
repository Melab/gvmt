/** As there are no generations, all calls delegate directly to the Policy.
*/
#include "gvmt/internal/gc_templates.hpp"
#include "gvmt/internal/gc_threads.hpp"
#include "gvmt/internal/memory.hpp"
#include "gvmt/internal/LargeObjectSpace.hpp"

#define MB ((unsigned)(1024*1024))

template <class Policy> class NonGenCollection {
public:
         
    static inline bool wants(GVMT_Object p) {
        return gc::is_address(p);
    }
    
    static inline GVMT_Object apply(GVMT_Object p) {
        assert(gc::is_address(p));
        Address a = Address(p);
        if (LargeObjectSpace::in(a))
            return LargeObjectSpace::grey(a);
        else
            return Policy::grey(a);
    }
    
    static inline bool is_live(Address p) {
        return Policy::is_live(p);
    }
    
    static inline void scanned(Address obj, Address end) {
        if (!LargeObjectSpace::in(obj))
            Policy::scanned(obj, end);
    }

};   
  
template <class Policy> class NonGenerational {
    
public:
   
    static inline GVMT_Object fast_allocate(size_t size) {
        if (size >= LARGE_OBJECT_SIZE) {
            return LargeObjectSpace::allocate(size, false);      
        } else {
            // Need to make this thread safe...
            return Policy::allocate(size).as_object();
        }
    }
    
    /** Do allocation, doing GC if necessary */
    static inline GVMT_Object allocate(GVMT_StackItem* sp, GVMT_Frame fp,
                                       size_t size) {
        void *mem = fast_allocate(size);
        if (mem == NULL) {
            mutator::request_gc();
            mutator::wait_for_collector(sp, fp);
            if (size >= LARGE_OBJECT_SIZE) {
                return LargeObjectSpace::allocate(size, true);      
            } else {
                // Need to make this thread safe...
                return Policy::allocate(size).as_object();
            }
            // Cannot run out of memory?
            assert (mem != NULL);
        }
        return (GVMT_Object)mem;
    }
    
    static inline void init(size_t heap_size_hint) {
        Policy::init(heap_size_hint);
        Heap::init<Policy>();
        Heap::ensure_space(4 * MB);
        GC::weak_references.intialise();    
        LargeObjectSpace::init();
        mutator::init();
        collector::init();
        finalizer::init();
    }
   
    /** Called by mutator code to pin an object */
    static inline void* pin(GVMT_Object obj) {
        return Policy::pin(obj);
    }

    static inline void collect() {
        int64_t t0, t1;
        t0 = high_res_time();
        Policy::pre_collection();
        LargeObjectSpace::pre_collection();
        HugeObjectSpace::pre_collection();
        gc::process_roots<NonGenCollection<Policy> >();
        gc::transitive_closure<NonGenCollection<Policy> >();
        gc::process_finalisers<NonGenCollection<Policy> >();
        gc::transitive_closure<NonGenCollection<Policy> >();
        gc::process_weak_refs<NonGenCollection<Policy> >();
        LargeObjectSpace::sweep();
        HugeObjectSpace::sweep();
        Policy::reclaim();
        Heap::done_collection();
        allocator::zero_limit_pointers();
        assert(GC::mark_stack_is_empty());
        t1 = high_res_time();
        gvmt_major_collections++;
        gvmt_major_collection_time += (t1 - t0);
        gvmt_total_collection_time += (t1 - t0);
    }
    
};

