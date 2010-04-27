/** Immix collector class.
 *  Implements Policy interface
 */
 
#ifndef GVMT_INTERNAL_IMMIX_H 
#define GVMT_INTERNAL_IMMIX_H 

#include <deque>
#include "gvmt/internal/gc.hpp"
#include "gvmt/internal/gc_templates.hpp"
#include "gvmt/internal/memory.hpp"

enum {
    FREE = 1,
    RECYCLE = 2,
    FULL = 3,
    IN_USE = 4
};

struct BlockData {
    uint8_t use;
    uint8_t free_lines;
    char pad[2];
};

class Immix {

    static float heap_residency;
    static int available_space_estimate;
    static std::vector<Block*> recycle_blocks;
    static size_t next_block_index;
    static Address free_ptr;
    static Address limit_ptr;
    static Address reserve_ptr;
    
    static inline uint8_t* mark_byte(Address addr) {
        Zone *z = Zone::containing(addr);
        uintptr_t index = Zone::index_of<Line>(addr);
        return &z->collector_line_data[index]; 
    }
    
    static inline bool line_marked(Address addr) {
        return *mark_byte(addr) != 0;
    }
    
    static void clear_mark_lines(Block* b) {
        Address a = b->start();
        Address end = b->end();
        for(; a < end; a = a.plus_bytes(Line::size)) {
            *mark_byte(a) = 0; 
        }
    }
    
    static inline BlockData* get_block_data(Block* b) {
        Zone *z = Zone::containing(b);   
        uintptr_t index = Zone::index_of<Block>(b);
        uint32_t* bd = &z->collector_block_data[index];
        assert(sizeof(BlockData) <= sizeof(uint32_t));
        return reinterpret_cast<BlockData*>(bd);
    }
    
    static inline void set_block_use(Block* b, char use) {
        BlockData* bd = get_block_data(b);   
        bd->use = use;
    }
    
    static inline char get_block_use(Block* b) {
        BlockData* bd = get_block_data(b);   
        return bd->use;
    }
  
    static Block* get_block() {
        Block* b = Heap::get_block(Space::MATURE, true);
        clear_mark_lines(b);
        get_block_data(b)->free_lines = Block::size/Line::size;
        set_block_use(b, IN_USE);
        sanity();
        return b;
    }
    
    static inline void find_new_hole() {
        // This can be inproved with some bit-twiddiling.
        // Just lookling for start and length of sequence of zeroes.
        sanity();
        if (!Line::starts_at(free_ptr))
            free_ptr.write_word(0);
        free_ptr = limit_ptr;
        while (!Block::starts_at(free_ptr) && line_marked(free_ptr))
            free_ptr = free_ptr.plus_bytes(Line::size);
        if (Block::starts_at(free_ptr)) {
            Block* b;
            if (next_block_index >= recycle_blocks.size()) {
                b = get_block();
            } else {
                b = recycle_blocks[next_block_index];
                next_block_index++;
                assert(get_block_use(b) == RECYCLE);
                available_space_estimate -= get_block_data(b)->free_lines * Line::size;
            }
            free_ptr = b->start();
            while (*mark_byte(free_ptr) != 0)
                free_ptr = free_ptr.plus_bytes(Line::size);
        }
        assert(!line_marked(free_ptr));
        limit_ptr = free_ptr.plus_bytes(Line::size);
        while (!Block::starts_at(limit_ptr) && !line_marked(limit_ptr)) {
            limit_ptr = limit_ptr.plus_bytes(Line::size);
        }
        assert(Block::starts_at(limit_ptr) || line_marked(limit_ptr));
        sanity();
    }
    
 public:
    
    static void reclaim(Block* b) {
        unsigned free_lines = 0;
        unsigned index, start = Zone::index_of<Line>(b);
        Zone *z = Zone::containing(b->start());
        for(index = 0; index < Block::size/Line::size; ++index) {
            if (z->collector_line_data[start+index] == 0) {
                free_lines += 1;
            }
        }
        get_block_data(b)->free_lines = free_lines;
        if (free_lines == 0) {
            set_block_use(b, FULL);
        } else if (free_lines == Block::size/Line::size) {
            Heap::free_blocks(b, 1);
            return;
        } else {
            set_block_use(b, RECYCLE);  
            recycle_blocks.push_back(b);
            available_space_estimate += free_lines * Line::size;
        }
    }
    
#ifdef NDEBUG
    static inline void sanity() { }
#else
    static void sanity() {
        assert(available_space_estimate >= 0);  
        std::vector<Zone*>::iterator it, end;
        for(Heap:: iterator it = Heap::begin(); it != Heap::end(); ++it) {
            for (Block* b = (*it)->first(); b != (*it)->end(); b++) {
                if (b->space() == Space::MATURE) {
                    int use = get_block_use(b);
                    assert(use >= RECYCLE && use <= IN_USE);
                }
            }
        }
    }
#endif
     
    static void data_block(Block* b) {
        set_block_use(b, FULL);
        assert(b->space() == Space::MATURE);
    }

     /** Called once during VM initialisation */
    static void init(size_t heap_size_hint, float residency) {
        if (residency == 0.0)
            heap_residency = 0.4;
        else if (residency > 0.6)
            heap_residency = 0.6;
        else
            heap_residency = residency;
        sanity();
    }
    
    /** Prepare for collection - All objects should be white.*/
    static void pre_collection() {
        std::vector<Zone*>::iterator it, end;
        for(Heap::iterator it = Heap::begin(); it != Heap::end(); ++it) {
            for (Block* b = (*it)->first(); b != (*it)->end(); b++) {
                if (b->space() == Space::MATURE) {
                    b->clear_mark_map();
                    b->clear_modified_map();
                    clear_mark_lines(b);
                }
            }
        }   
        available_space_estimate = 0;
        recycle_blocks.clear();
        free_ptr = 0;
        limit_ptr = 0;
        reserve_ptr = 0;
        next_block_index = 0;
        sanity();
    }
    
    static inline Address allocate(size_t size) {
        Address allocated;
        if (free_ptr.plus_bytes(size) > limit_ptr) {
            if (size > Line::size) {
                if (!Block::can_allocate(reserve_ptr, size)) {
                    if (!Block::starts_at(reserve_ptr))
                        reserve_ptr.write_word(0);
                    Block *b = get_block();
                    reserve_ptr = b->start();
                }
                allocated = reserve_ptr;
                reserve_ptr = reserve_ptr.plus_bytes(size);
                assert(allocated != Address());
                return allocated;
            } else {
                find_new_hole();
            }
        }
        assert(free_ptr.plus_bytes(size) <= limit_ptr);
        allocated = free_ptr;
        free_ptr = free_ptr.plus_bytes(size);
        assert(allocated != Address());
        return allocated;
    }

    /** Return true if this object is grey or black */
    static inline bool is_live(GVMT_Object obj) {
        return Zone::marked(gc::untag(obj));
    }
     
    /** Returns the available space in bytes */
    static inline size_t available_space() {
        return available_space_estimate < 0 ? 0 : available_space_estimate;
    }
    
    static inline void scanned(Address obj, Address end) {
        Zone* z = Zone::containing(obj);
        Line* l = Line::containing(obj);
        do {
            z->collector_line_data[Zone::index_of<Line>(l)] = 1;
            l = l->next();
        } while (l->start() < end);
    }
    
    /** Mark this object as grey, that is live, but not scanned */
    static inline GVMT_Object grey(GVMT_Object obj) {
        Address addr = gc::untag(obj);
        if (!Zone::marked(addr)) {
            Zone::mark(addr);
            GC::push_mark_stack(addr);
        }
        return obj;
    }
      
    static void promote_pinned_block(Block *b) {
        assert(b->space() == Space::PINNED);   
        b->set_space(Space::MATURE);
        Zone* z = Zone::containing(b);
        // Copy pinned to mark, then "reclaim" this block.
        unsigned start = Zone::index_of<Line>(b);
        uint8_t* from = &z->pinned[start];
        uint8_t* to = &z->collector_line_data[start];
        memcpy(to, from, Block::size/Line::size);
        reclaim(b);
    }
    
    static inline void pin(GVMT_Object obj) {
        // Nothing to do - as yet.
    }
    
    static inline bool is_scannable(Address obj) {
        return Zone::marked(obj);
    }
    
};

float Immix::heap_residency;
int Immix::available_space_estimate;
std::vector<Block*> Immix::recycle_blocks;
size_t Immix::next_block_index;
Address Immix::free_ptr;
Address Immix::limit_ptr;
Address Immix::reserve_ptr;


#endif // GVMT_INTERNAL_IMMIX_H 

