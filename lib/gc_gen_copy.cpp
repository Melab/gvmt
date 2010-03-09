/** Generational copying collector using GC framework */

#include "gvmt/internal/gc.hpp"
#include "gvmt/internal/gc_threads.hpp"
#include "gvmt/internal/gc_templates.hpp"
#include "gvmt/internal/gc_semispace.hpp"
#include "gvmt/internal/gc_generational.hpp"

namespace gen_copy {
    char name[] = "gen_copy";
    Card current_card;
}

class CopyBase {
  public: 
    
    static inline GVMT_Object apply(GVMT_Object p) {
        if (semispace::forwarded(p)) {
            return semispace::forwarding_address(p);
        }
        GVMT_Object to = semispace::move(p);
        Address addr = gc::untag(to);
        Card card = generational::card_id(addr);
        if (card != gen_copy::current_card) {
            gen_copy::current_card = card;
            generational::set_first_object_on_card(card, addr);
        }
        return to;
    }
    
    static inline bool is_live(GVMT_Object p) {
        return semispace::forwarded(p);
    }
    
};

class CopyMinor: public CopyBase { 
public:
    
    static inline bool wants(GVMT_Object p) {
        return generational::in_nursery(p);
    }
};

class CopyMajor : public CopyBase { 
public:
    
    static inline bool wants(GVMT_Object p) {
        if (p == NULL) {
            return false;
        }
        return !gc::opaque(p);        
    }
};

namespace gen_copy {
    
    void minor_collect(void) {
        int64_t t0, t1;
        t0 = high_res_time();
        char* old_top = semispace::free;
        gc::process_roots<CopyMinor>();
        generational::process_old_young<CopyMinor>(semispace::to_space, old_top);
        semispace::scan<CopyMinor>();
        gc::process_finalisers<CopyMinor>();
        semispace::scan<CopyMinor>();
        gc::process_weak_refs<CopyMinor>();
        generational::clear_cards(semispace::to_space, semispace::free);
        allocator::free = generational::nursery;
        allocator::limit = generational::nursery_top;
        t1 = high_res_time();
        gvmt_minor_collections++;
        gvmt_minor_collection_time += (t1 - t0);
        gvmt_total_collection_time += (t1 - t0);
    }
    
    void major_collect(void) {
        int64_t t0, t1;
        t0 = high_res_time();
        semispace::swap_spaces();
        current_card = 0;
        gc::process_roots<CopyMajor>();
        semispace::scan<CopyMajor>();
        gc::process_finalisers<CopyMajor>();
        semispace::scan<CopyMajor>();
        gc::process_weak_refs<CopyMajor>();
        generational::clear_cards(semispace::to_space, semispace::free);
        allocator::free = generational::nursery;
        allocator::limit = generational::nursery_top;
        t1 = high_res_time();
        gvmt_major_collections++;
        gvmt_major_collection_time += (t1 - t0);
        gvmt_total_collection_time += (t1 - t0);
    }
};

void gvmt_do_collection(void) {
    gen_copy::minor_collect();
    if (semispace::free + generational::nursery_size >= semispace::top_of_space) 
        gen_copy::major_collect();
//    if (semispace::free + generational::nursery_size >= semispace::top_of_space) 
//        gvmt_user_out_of_memory();
}

/** GVMT GC interface */
extern "C" {

    char* gvmt_gc_name = &gen_copy::name[0];
    
    void gvmt_malloc_init(uint32_t s, float residency) {
        int64_t t0, t1;
        t0 = high_res_time();
        generational::init(s);
        semispace::init(generational::heap, generational::nursery-generational::heap);
        gen_copy::current_card = 0;
        gc::process_roots<CopyMajor>();
        semispace::scan<CopyMajor>();
        allocator::free = generational::nursery;
        allocator::limit = generational::nursery_top;
        finalizer::init();
        collector::init();
        t1 = high_res_time();
        gvmt_total_collection_time += (t1 - t0);
    }
    
    GVMT_Object gvmt_gen_copy_malloc(GVMT_StackItem* sp, GVMT_Frame fp, uintptr_t size) {
        return (GVMT_Object)allocator::malloc(sp, fp, size);
    }
    
    GVMT_LINKAGE_1 (gvmt_gc_pin, void* obj)
        fprintf(stderr, "Pinning not supported\n");
        abort();
        GVMT_RETURN_P(obj);
    }
    
    GVMT_LINKAGE_1 (gvmt_gc_unpin, void* obj)
        (void)obj; // Use obj to keep compiler quiet
        fprintf(stderr, "Pinning not supported\n");
        abort();
        GVMT_RETURN_V;
    }
}
