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
        return p != NULL;
    }
    
    static inline GVMT_Object apply(GVMT_Object p) {
        if (p == NULL)
            return NULL;
        if (LargeObjectSpace::in(p))
            return LargeObjectSpace::grey(p);
        else
            return Policy::grey(p);
    }
    
    static inline bool is_live(GVMT_Object p) {
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
            return LargeObjectSpace::allocate(size);      
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
            gvmt_gc_waiting = true;
            mutator::wait_for_collector(sp, fp);
            mem = fast_allocate(size);
            // Cannot run out of memory?
            assert (mem != NULL);
        }
        return (GVMT_Object)mem;
    }
    
    static inline void init(size_t heap_size_hint, float residency) {
        Policy::init(heap_size_hint, residency);
        Memory::heap_init<Policy>();
        Policy::ensure_space(4 * MB);
        GC::finalizers.intialise();
        GC::weak_references.intialise();    
        LargeObjectSpace::init();
        finalizer::init();
        collector::init();
    }
   
    /** Called by mutator code to pin an object */
    static inline void* pin(GVMT_Object obj) {
        return Policy::pin(obj);
    }

    static inline void collect() {
        int64_t t0, t1;
        t0 = high_res_time();
        Policy::pre_collection();
        gc::process_roots<NonGenCollection<Policy> >();
        gc::transitive_closure<NonGenCollection<Policy> >();
        gc::process_finalisers<NonGenCollection<Policy> >();
        gc::transitive_closure<NonGenCollection<Policy> >();
        gc::process_weak_refs<NonGenCollection<Policy> >();
        LargeObjectSpace::sweep();
        Policy::reclaim();
        allocator::zero_limit_pointers();
        assert(GC::mark_stack.empty());
        t1 = high_res_time();
        gvmt_major_collections++;
        gvmt_major_collection_time += (t1 - t0);
        gvmt_total_collection_time += (t1 - t0);
    }

};

