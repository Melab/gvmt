#include <algorithm>
#include "gvmt/internal/gc_templates.hpp"
#include "gvmt/internal/gc_threads.hpp"
#include "gvmt/internal/memory.hpp"
#include "gvmt/internal/LargeObjectSpace.hpp"

#define MB ((unsigned)(1024*1024))

class Nursery {

    static inline GVMT_Object bump_pointer_alloc(int asize) {
        char* result = (char*)gvmt_gc_free_pointer;
        gvmt_gc_free_pointer = (GVMT_StackItem*)(((char*)result) + asize);
        assert(Space::is_young(result));
        assert(!Block::crosses(result, asize));
        assert(Zone::valid_address(result));
        return (GVMT_Object)result;
    }
    
    static std::vector<Block*> blocks;
    static std::vector<Block*> pinned;
    static size_t next_free_block_index;
    
    static Block* get_block_synchronised() {
        size_t block_index;
        do {
            block_index = next_free_block_index;
        } while (!COMPARE_AND_SWAP(&next_free_block_index, block_index, block_index+1));
        assert(next_free_block_index > block_index);
        if (block_index < blocks.size()) {
            gvmt_bytes_passed_to_allocators += Block::size;
            Block* result = blocks[block_index];
            assert(result->is_valid());
            return result;
        } else {
            return NULL;
        }
    }
    
    
#ifndef NDEBUG
    static void sanity() {
        // Sanity check:
        std::vector<Block*>::iterator it;
        for (it = blocks.begin(); it != blocks.end(); it++) {
            Zone *z = Zone::containing(*it);
            assert((*it)->space() == Space::NURSERY);
            int block_index = Zone::index_of<Block>(*it);
            int idx = z->collector_block_data[block_index];
            assert(blocks[idx] == *it);
        }
        for (it = pinned.begin(); it != pinned.end(); it++) {
            assert((*it)->space() == Space::PINNED);
            assert((*it)->is_pinned());
        }
    }
#else
    static inline void sanity() {}
#endif

public:
    
    static bool any_pinned() {
        return pinned.size() != 0; 
    }
    
    static void add_block(Block* b) {
        int index = Zone::index_of<Block>(b);
        Zone::containing(b)->collector_block_data[index] = blocks.size();
        b->set_space(Space::NURSERY);
        blocks.push_back(b);
        gvmt_nursery_size += Block::size;
        sanity();
    }

    static void resize(size_t size_hint) {
        if (size_hint < MB)
            size_hint = MB;
        while (gvmt_nursery_size < size_hint) {
            add_block(Heap::get_block(Space::NURSERY, true));
        }
    }
    
    static void pin(Block* b) {
        assert(b->space() == Space::PINNED);
        b->set_pinned(true);
        assert(find(blocks.begin(), blocks.end(), b) != blocks.end());
        int index = Zone::index_of<Block>(b);
        int blocks_index = Zone::containing(b)->collector_block_data[index];
        Block* replace = blocks.back();
        assert(blocks[blocks_index] == b);
        blocks[blocks_index] = replace;
        int r_index = Zone::index_of<Block>(replace);
        Zone::containing(replace)->collector_block_data[r_index] = blocks_index;
        blocks.pop_back();
        pinned.push_back(b);
        assert(find(blocks.begin(), blocks.end(), b) == blocks.end());
        sanity();
    }
    
    static void clear() {
        next_free_block_index = 0;
        allocator::zero_limit_pointers();
    }
    
    static GVMT_Object allocate(size_t size) {
        size_t asize = align(size);
        assert(asize < Block::size);
        intptr_t bits = (intptr_t)gvmt_gc_free_pointer;
        if (size > (((uintptr_t)(-bits)) & (Block::size-1))) {
            Block *b = get_block_synchronised();
            if (b == NULL) 
                return NULL;
            assert(b->space() == Space::NURSERY);
            gvmt_gc_free_pointer = (GVMT_StackItem*)b;
        }
        return bump_pointer_alloc(asize);
    }
         
    /** Need to test if gvmt_gc_free_pointer is on Block boundary or would cross
     * one when a size is added. */
    static inline GVMT_Object fast_allocate(size_t size) {
        size_t asize = align(size);
        assert(asize < Block::size);
        intptr_t bits = (intptr_t)gvmt_gc_free_pointer;
        if (size <= (((uintptr_t)(-bits)) & (Block::size-1))) {
            assert(!Block::crosses((char*)gvmt_gc_free_pointer, asize));
            assert(Block::containing((char*)gvmt_gc_free_pointer) != (Block*)gvmt_gc_free_pointer);
            return bump_pointer_alloc(asize);
        } else {
            return NULL;   
        }
    }
    
    /** Promotes pinned blocks to the Mature space and returns the 
     * number of promoted blocks */
    template <class Policy> static int promote_pinned_blocks() {
        sanity();
        std::vector<Block*>::iterator it;
        int size = pinned.size();
        while (!pinned.empty()) {
            Block *b = pinned.back();
            assert(b->is_pinned());
            pinned.pop_back();
            assert(find(blocks.begin(), blocks.end(), b) == blocks.end());
            sanity();
            Policy::promote_pinned_block(b);
            sanity();
            b->clear_modified_map();
            sanity();
        }
        sanity();
        gvmt_nursery_size -= Block::size * size;
        return size;
    }
    
};
  
size_t Nursery::next_free_block_index = 0;
std::vector<Block*> Nursery::blocks;
std::vector<Block*> Nursery::pinned;

template <class Policy> class MajorCollection {
public:
    
    typedef Policy policy;
    
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
        if (LargeObjectSpace::in(p))
            return LargeObjectSpace::is_live(p);
        else
            return Policy::is_live(p);
    }
    
    static inline void scanned(Address obj, Address end) {
        if (!LargeObjectSpace::in(obj)) {
            assert(Block::containing(obj)->space() == Space::MATURE);
            Policy::scanned(obj, end);
        }
    }
    
};

template <class Policy> class MinorCollection {
public:
    
    typedef Policy policy;
    
    static inline bool wants(GVMT_Object p) {
        assert (!gc::is_address(p) || Block::containing(p)->space() != Space::PINNED);
        return gc::is_address(p) && Space::is_young(Address(p));
    }
    
    static inline GVMT_Object apply(GVMT_Object p) {
        assert(gc::is_address(p));
        return Memory::copy<Policy>(Address(p));
    }
    
    static inline bool is_live(Address p) {
        return Memory::forwarded(p);
    }
    
    static inline void scanned(Address obj, Address end) {
    }

};

template <class Policy> class MinorCollectionWithPinning {
public:
    
    typedef Policy policy;
    
    static inline bool wants(GVMT_Object p) {
        return gc::is_address(p) && Space::is_young(Address(p));
    }
    
    static inline GVMT_Object apply(GVMT_Object p) {
        assert(gc::is_address(p));
        if (Block::containing(p)->space() == Space::PINNED) {
            assert(gc::is_address(p));
            Address addr = Address(p);
            if (!Zone::marked(addr)) {
                Zone::mark(addr);
                GC::push_mark_stack(addr);
            }
            return p;
        } else {
            return Memory::copy<Policy>(Address(p));
        }
    }
    
    static inline bool is_live(Address p) {
        if (Block::containing(p)->space() == Space::PINNED)
            return Zone::marked(p);
        else
            return Memory::forwarded(p);
    }
    
    static inline void scanned(Address obj, Address end) {
        if (Block::containing(obj)->space() == Space::PINNED) {
            Policy::scanned(obj, end);
        }
    }

};

template <class Policy> class Generational {
    
    static uint32_t nursery_shortfall;
    
public:
        
    /** Do allocation, return NULL if cannot allocate.
     * Do not do any more than a small, bounded amount of processing here. 
     */
    static inline GVMT_Object fast_allocate(size_t size) {
        GVMT_Object result = Nursery::fast_allocate(size); 
        if (result != NULL)
            return result;
        if (size >= LARGE_OBJECT_SIZE) {
            return NULL;
        } else {
            return Nursery::allocate(size);
        }
    }
    
    /** Do allocation, doing GC if necessary */
    static inline GVMT_Object allocate(GVMT_StackItem* sp, GVMT_Frame fp,
                                       size_t size) {
        GVMT_Object mem;
        if (size >= LARGE_OBJECT_SIZE) {
            mem = LargeObjectSpace::allocate(size, false);  
        } else {
            mem = Nursery::allocate(size);
        }
        if (mem == NULL) {
            mutator::request_gc();
            mutator::wait_for_collector(sp, fp);
            if (size >= LARGE_OBJECT_SIZE) {
                mem = LargeObjectSpace::allocate(size, true);  
            } else {
                mem = Nursery::allocate(size);
            }
            assert (mem != NULL);
        }
        return mem;
    }
    
    static inline void init(size_t heap_size_hint) {
        int64_t t0, t1;
        t0 = high_res_time();
        Nursery::resize(0);
        Policy::init(heap_size_hint);
        Heap::init<Policy>();
        Heap::ensure_space(std::max(gvmt_nursery_size, 4*MB));
        GC::weak_references.intialise();
        LargeObjectSpace::init();
        mutator::init();
        collector::init();
        finalizer::init();
        t1 = high_res_time();
        gvmt_total_collection_time += (t1 - t0);
    }
   
    /** Called by mutator code to pin an object */
    static inline void* pin(GVMT_Object obj) {
        int block_index = Zone::index_of<Block>(Address(obj));
        Zone* z = Zone::containing(obj);
        char* addr = &z->spaces[block_index];
        char space = *addr;
        if (space != Space::PINNED) {
            if (space == Space::NURSERY) {
                if (COMPARE_AND_SWAP_BYTE(addr, Space::NURSERY, Space::PINNED)) {
                    Nursery::pin(Block::containing(obj));
                }
                z->pinned[Zone::index_of<Line>(Address(obj))] = 1;
                assert(Block::containing(obj)->space() == Space::PINNED); 
            } else {
                if (space == Space::MATURE) {
                    Policy::pin(obj);
                // Large objects are already pinned
                } else {
                    assert(space == Space::LARGE);
                }
            }
        } else {
            z->pinned[Zone::index_of<Line>(Address(obj))] = 1;
        }
        assert(is_pinned(obj));
        return reinterpret_cast<void*>(obj);
    }
    
    static int is_pinned(void* ptr) {
        if (gc::is_tagged(ptr)) {
            // Pinned means ptr is unmodifiable by GC.
            // Therefore tagged values are pinned.
            return 1;
        }
        if (LargeObjectSpace::in((char*)ptr))
            return 1;
        Zone* z = Zone::containing(ptr);
        Block* b = Block::containing(ptr);
        if (b->space() == Space::MATURE && !Zone::marked((char*)ptr))
            return 0;
        return b->is_pinned() &&
            z->pinned[Zone::index_of<Line>((Line*)ptr)];
    }
        
    template <class C> static inline void process_old_young(Block* b) {
        Zone* z = Zone::containing(b);
        Line* line = reinterpret_cast<Line*>(b);
        for (int i = 0; i < CARDS_PER_BLOCK; i++) {
            if (z->modified(line)) {
                Zone::scan_marked_objects<C>(line);
                z->clear_modified(line);
            }
            line = line->next();
        }
    }
            
    template <class C> static inline void process_old_young() {
        Heap::iterator end = Heap::end();
        for(Heap::iterator it = Heap::begin(); it != end; ++it) {
            for (Block* b = (*it)->first(); b != (*it)->first_virtual(); b++) {
                if (b->space() == Space::MATURE)
                    process_old_young<C>(b);
            }
        }
        LargeObjectSpace::process_old_young<C>();
        HugeObjectSpace::process_old_young<C>();
    }
    
    static void minor_collect() {        
        int64_t t0, t1;
        t0 = high_res_time();
        if (Nursery::any_pinned()) {
            gc::process_roots<MinorCollectionWithPinning<Policy> >();
            process_old_young<MinorCollectionWithPinning<Policy> >();
            gc::transitive_closure<MinorCollectionWithPinning<Policy> >();
            gc::process_finalisers<MinorCollectionWithPinning<Policy> >();
            gc::transitive_closure<MinorCollectionWithPinning<Policy> >();
            gc::process_weak_refs<MinorCollectionWithPinning<Policy> >();
            nursery_shortfall += Nursery::promote_pinned_blocks<Policy>();
        } else {
            gc::process_roots<MinorCollection<Policy> >();
            process_old_young<MinorCollection<Policy> >();
            gc::transitive_closure<MinorCollection<Policy> >();
            gc::process_finalisers<MinorCollection<Policy> >();
            gc::transitive_closure<MinorCollection<Policy> >();
            gc::process_weak_refs<MinorCollection<Policy> >();
        }
        Nursery::clear();
        while(nursery_shortfall) {
            Nursery::add_block(Heap::get_block(Space::NURSERY, true));
            --nursery_shortfall; 
        }
        assert(GC::mark_stack.empty());
        t1 = high_res_time();
        gvmt_minor_collections++;
        gvmt_minor_collection_time += (t1 - t0);
        gvmt_total_collection_time += (t1 - t0);
    }
    
    static void major_collect() {
        int64_t t0, t1;
        t0 = high_res_time();
        Policy::pre_collection();
        LargeObjectSpace::pre_collection();
        HugeObjectSpace::pre_collection();
        gc::process_roots<MajorCollection<Policy> >();
        gc::transitive_closure<MajorCollection<Policy> >();
        gc::process_finalisers<MajorCollection<Policy> >();
        gc::transitive_closure<MajorCollection<Policy> >();
        gc::process_weak_refs<MajorCollection<Policy> >();
        LargeObjectSpace::sweep();
        HugeObjectSpace::sweep();
        Policy::reclaim();
        Heap::done_collection();
        assert(GC::mark_stack.empty());
        size_t nursery = std::min(gvmt_real_heap_size / 4, gvmt_nursery_size *2);
        Nursery::resize(std::min(nursery, 8*MB));
        Heap::ensure_space(gvmt_nursery_size - Policy::available_space());
        t1 = high_res_time();            
        gvmt_major_collections++;
        gvmt_major_collection_time += (t1 - t0);
        gvmt_total_collection_time += (t1 - t0);
    }

    static inline void collect() {
        Policy::sanity();
        minor_collect();
        Policy::sanity();
        if (Heap::available_space() + Policy::available_space() < gvmt_nursery_size ||
            HugeObjectSpace::allocated_space_since_collection() > gvmt_nursery_size) {
            Policy::sanity();
            major_collect();
            Policy::sanity();
        }
    }

    static inline void full_collect() {
        Policy::sanity();
        minor_collect();
        Policy::sanity();
        major_collect();
        Policy::sanity();
    }

};

template <class Policy> uint32_t Generational<Policy>::nursery_shortfall = 0;

extern "C" {
 
    void gvmt_gen_write_barrier(char* obj, size_t offset) {
        Zone *z = Zone::containing(obj);
        assert(Zone::valid_address(obj));
        assert(Zone::valid_address(obj+offset));
        assert(z == Zone::containing(obj+offset));
        assert(Zone::index_of<Block>(obj) >= 2);
        assert(Zone::index_of<Block>(obj+offset) >= 2);
        uint8_t* mod =z->modification_byte(Line::containing(obj));
        assert(Zone::index_of<Block>(Block::containing(mod)) == 0);
        *mod = 1;        
    }
    
}

