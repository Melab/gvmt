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
    static size_t available_space_estimate;
    static std::deque<Block*> free_blocks;
    static std::vector<Block*> all_blocks;
    static BlockManager<Space::MATURE> manager;
    static size_t next_block_index;
    static Address free_ptr;
    static Address limit_ptr;
    static Address reserve_ptr;
    
    static inline uint8_t* mark_byte(Address addr) {
        SuperBlock *sb = SuperBlock::containing(addr);
        uintptr_t index = SuperBlock::index_of<Line>(addr);
        return &sb->collector_line_data[index]; 
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
        SuperBlock *sb = SuperBlock::containing(b);   
        uintptr_t index = SuperBlock::index_of<Block>(b);
        uint32_t* bd = &sb->collector_block_data[index];
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
        Block* b;
        if (free_blocks.empty()) {
            b = manager.allocate_block();
            clear_mark_lines(b);
            get_block_data(b)->free_lines = Block::size/Line::size;
            all_blocks.push_back(b);
        } else {
            b = free_blocks.front();
            free_blocks.pop_front();
        }
        set_block_use(b, IN_USE);
        sanity();
        return b;
    }
    
    static void reclaim(Block* b) {
        unsigned free_lines = 0;
        unsigned index, start = SuperBlock::index_of<Line>(b);
        SuperBlock *sb = SuperBlock::containing(b->start());
        for(index = 0; index < Block::size/Line::size; ++index) {
            if (sb->collector_line_data[start+index] == 0) {
                free_lines += 1;
            }
        }
        get_block_data(b)->free_lines = free_lines;
        available_space_estimate += free_lines * Line::size;
        if (free_lines == 0) {
            set_block_use(b, FULL);
        } else if (free_lines == Block::size/Line::size) {
            set_block_use(b, FREE);
            free_blocks.push_back(b);
        } else {
            set_block_use(b, RECYCLE);  
        }
    }
    
    static inline void find_new_hole() {
        sanity();
        if (!Line::starts_at(free_ptr))
            free_ptr.write_word(0);
        free_ptr = limit_ptr;
        while (!Block::starts_at(free_ptr) && line_marked(free_ptr))
            free_ptr = free_ptr.plus_bytes(Line::size);
        if (Block::starts_at(free_ptr)) {
            Block* b;
            do {
                if (next_block_index >= all_blocks.size()) {
                    b = get_block();
                    break;
                }
                b = all_blocks[next_block_index];
                next_block_index++;
            } while (get_block_use(b) != RECYCLE);
            available_space_estimate -= get_block_data(b)->free_lines * Line::size;
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
    
#ifdef NDEBUG
    static inline void sanity() { }
#else
    static void sanity() {
        //Check all block in free queue are free and do in fact match those in all_blocks.   
        std::vector<Block*>::iterator it;
        unsigned free_index = 0;
        for (it = all_blocks.begin(); it != all_blocks.end(); ++it) {
            int use = get_block_use(*it);
            assert(use >= FREE && use <= IN_USE);
            if (use == FREE) {
                assert(*it == free_blocks[free_index]);   
                ++free_index;
            }
        }
        assert(free_index == free_blocks.size());
    }
#endif
    
 public:
     
    static void data_block(Block* b) {
        set_block_use(b, FULL);
        all_blocks.push_back(b);
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
        std::vector<Block*>::iterator it;
        for (it = all_blocks.begin(); it != all_blocks.end(); ++it) {
             (*it)->clear_mark_map();
             (*it)->clear_modified_map();
             clear_mark_lines(*it);
        }   
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
                    available_space_estimate -= Block::size;
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
        return SuperBlock::marked(gc::untag(obj));
    }
     
    /** Returns the available space in bytes */
    static inline size_t available_space() {
        return available_space_estimate;
    }
    
    /** Reclaim all white objects */
    static void reclaim() {
        sanity();
        free_blocks.clear();
        std::vector<Block*>::iterator it;
        for (it = all_blocks.begin(); it != all_blocks.end(); ++it) {
             reclaim(*it);
        }
        sanity();
    }
    
    static inline void scanned(Address obj, Address end) {
        SuperBlock* sb = SuperBlock::containing(obj);
        Line* l = Line::containing(obj);
        do {
            sb->collector_line_data[SuperBlock::index_of<Line>(l)] = 1;
            l = l->next();
        } while (l->start() < end);
    }
    
    /** Mark this object as grey, that is live, but not scanned */
    static inline GVMT_Object grey(GVMT_Object obj) {
        Address addr = gc::untag(obj);
        if (!SuperBlock::marked(addr)) {
            SuperBlock::mark(addr);
            GC::push_mark_stack(addr);
        }
        return obj;
    }
       
    static inline std::vector<Block*>::iterator begin_blocks() {
        if (!Block::starts_at(reserve_ptr))
            reserve_ptr.write_word(0);
        if (!Line::starts_at(free_ptr))
            free_ptr.write_word(0);
        sanity();
        return all_blocks.begin();
    }
    
    static inline std::vector<Block*>::iterator end_blocks() {
        return all_blocks.end();
    }

    static void ensure_space(uintptr_t space) {
        while (available_space_estimate < space) {
            Block* b = manager.allocate_block();
            clear_mark_lines(b);
            get_block_data(b)->free_lines = Block::size/Line::size;
            set_block_use(b, FREE);
            all_blocks.push_back(b);
            free_blocks.push_back(b);
            available_space_estimate += Block::size;
        }
    }
    
    static void promote_pinned_block(Block *b) {
        assert(b->space() == Space::PINNED);   
        b->set_space(Space::MATURE);
        all_blocks.push_back(b);
        SuperBlock* sb = SuperBlock::containing(b);
        // Copy pinned to mark, then "reclaim" this block.
        unsigned start = SuperBlock::index_of<Line>(b);
        uint8_t* from = &sb->pinned[start];
        uint8_t* to = &sb->collector_line_data[start];
        memcpy(to, from, Block::size/Line::size);
        reclaim(b);
    }
    
    static Block* release_block() {
        if (free_blocks.empty()) {
            return manager.allocate_block();
        } else {
           Block* released = free_blocks.back();
           free_blocks.pop_back();
           return released;
        }
    }
    
    static inline void pin(GVMT_Object obj) {
        // Nothing to do - as yet.
    }
    
    static inline bool is_scannable(Address obj) {
        return SuperBlock::marked(obj);
    }
    
};

float Immix::heap_residency;
size_t Immix::available_space_estimate;
std::deque<Block*> Immix::free_blocks;
std::vector<Block*> Immix::all_blocks;
BlockManager<Space::MATURE> Immix::manager;
size_t Immix::next_block_index;
Address Immix::free_ptr;
Address Immix::limit_ptr;
Address Immix::reserve_ptr;


#endif // GVMT_INTERNAL_IMMIX_H 

