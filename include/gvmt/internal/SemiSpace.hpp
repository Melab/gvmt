/** SemiSpace Policy class.
  * Implements Policy interface.
*/
 
#ifndef GVMT_INTERNAL_SEMISPACE_H 
#define GVMT_INTERNAL_SEMISPACE_H 

#include <deque>
#include "gvmt/internal/gc.hpp"
#include "gvmt/internal/gc_templates.hpp"
#include "gvmt/internal/memory.hpp"

class SemiSpace {
    
    /** Two semi-spaces, each built from Blocks.
     * Never return blocks.
     */
    static size_t unused_memory;
    static std::vector<Block*> semispace1;
    static std::vector<Block*> semispace2;
    static std::vector<Block*>* to_space;
    static std::vector<Block*>* from_space;
    static size_t next_free_block_index;
    static Address copy_ptr;
    static Address scan_ptr;
    static float heap_residency;
    
    static void balance_spaces() {
        std::vector<Block*>* bigger_space;
        std::vector<Block*>* smaller_space;
        if (semispace1.size() < semispace2.size()) {
            bigger_space = &semispace2;
            smaller_space = &semispace1;
        } else {
            bigger_space = &semispace1;
            smaller_space = &semispace2;
        }
        while (bigger_space->size() > smaller_space->size()) {
             smaller_space->push_back(bigger_space->back());
             bigger_space->pop_back();
        }
        if (smaller_space->size() > bigger_space->size())
            smaller_space->pop_back();
        assert(smaller_space->size() == bigger_space->size());
        assert(next_free_block_index <= to_space->size());
    }
 
//    static inline GVMT_Object bump_pointer_alloc(int asize) {
//        char* next = (char*)gvmt_gc_free_pointer;
//        gvmt_gc_free_pointer = (GVMT_StackItem*)(((char*)next) + asize);
//        return (GVMT_Object)next;
//    }
    
    static inline Block* get_block_synchronised() {
        size_t block_index;
        do {
            block_index = next_free_block_index;
        } while (!COMPARE_AND_SWAP(&next_free_block_index, block_index, block_index+1));
        if (block_index < to_space->size())
            return (*to_space)[block_index];
        else
            return NULL;
    }
     
    static void init_heap() {
        Block* b = (Block*)&gvmt_start_heap;
        assert(b == Block::containing((char*)b));
        // Skip zone headers
        while (Zone::index_of<Block>((char*)b) < 2)
            b = b->next();
        uint8_t area;
        while ((area = b->space()) == Space::MATURE) {
            if (((intptr_t*)b)[0] != 0) {
                to_space->push_back(b);
            }
            b = b->next();
            // Skip zone headers
            if (Zone::index_of<Block>((char*)b) == 0) {
                b = b->next(); 
                b = b->next();
            }
        }
        next_free_block_index = to_space->size();
    }
    
 public:
     
#define MEGA_MINUS_ONE ((1 << 20) - 1)

    static void data_block(Block* b) {
        assert(b->is_valid());
        b->set_space(Space::MATURE);
        to_space->push_back(b);
        next_free_block_index = to_space->size();
    }
 
    /** Called once during VM initialisation */
    static void init(size_t heap_size_hint) {
        heap_residency = 0.3;
        copy_ptr = 0;
        scan_ptr = 0;
        // Roundup to nearest megabyte
        unused_memory = (heap_size_hint + MEGA_MINUS_ONE & (~MEGA_MINUS_ONE));
        to_space = &semispace1;
        from_space = &semispace2;
        from_space->push_back(Heap::get_block(Space::MATURE, true));
    }
    
//    /** Do allocation, return NULL if cannot allocate.
//     * Do NOT do any more than a small bounded amount of processing here. */
//    static inline GVMT_Object allocate(size_t size) {
//        int asize = align(size);
//        if (((char*)gvmt_gc_free_pointer) + asize < (char*)gvmt_gc_limit_pointer) {
//            return bump_pointer_alloc(asize);
//        }
//        Block *b = get_block_synchronised();
//        if (b == NULL) 
//            return NULL;
//        gvmt_gc_free_pointer = (GVMT_StackItem*)b;
//        return bump_pointer_alloc(asize);
//    }
    
    static inline Address allocate(size_t size) {
        if (!Block::can_allocate(copy_ptr, size)) {
            // Need to zero next word and
            // start of all remaining lines.
            if (!Block::starts_at(copy_ptr)) {
                copy_ptr.write_word(0);
                Line* line = Line::containing(copy_ptr)->next();
                while (!Block::starts_at((char*)line)) {
                    line->start().write_word(0);
                    line = line->next();
                }
            }
            assert(next_free_block_index < to_space->size());
            Block* copy_block = (*to_space)[next_free_block_index];
            next_free_block_index++;
            copy_ptr = copy_block->start();
            assert(copy_block->contains(copy_ptr.plus_bytes(size)));
        }
        assert(Zone::valid_address(copy_ptr));
        assert(Block::containing(copy_ptr)->is_valid());
        assert(Block::containing(copy_ptr)->space() == Space::MATURE);
        Address result = copy_ptr;
        copy_ptr = copy_ptr.plus_bytes(size);
        return result;
    }
    
    /** Returns the available space in bytes */
    static inline size_t available_space() {
        int blocks = to_space->size() - next_free_block_index;
        if (blocks > 0)
            return Block::size * (size_t)blocks;
        else
            return 0;
    }
    
    static inline void scanned(Address start, Address end) {
    }
    
    /** Prepare for collection - All objects should be white.*/
    static void pre_collection() {
        { //Swap spaces.   
            std::vector<Block*>* temp = from_space; 
            from_space = to_space; 
            to_space = temp;
        }
        while (to_space->size() < from_space->size()) {
            Block *b = Heap::get_block(Space::MATURE, false);
            if (b == NULL) {
                // TO DO -- out_of_memory();
                abort();
            }
            to_space->push_back(b);
        }
        std::vector<Block*>::iterator it;
        for (it = to_space->begin(); it != to_space->end(); ++it) {
            (*it)->clear_modified_map();
        }
        copy_ptr = 0;
        scan_ptr = 0;
        next_free_block_index = 0;
//        assert(to_be_scanned.empty());
    }
    
//    static void ensure_space(size_t headroom) {
//        size_t required_blocks = (headroom+Block::size-1)/Block::size
//                              + next_free_block_index;
//        while(to_space->size() < required_blocks) {
//            Block *b = Heap::get_block(Space::MATURE, false);
//            if (b == NULL) {
//                // No more memory available 
//                balance_spaces();
//                return;
//            }
//            to_space->push_back(b);
//        }
//    }
//    
    /** Reclaim all white objects */
    static void reclaim() {
        // Ensure that there is enough head room.
        // Either 120 blocks or 50%, which ever is greater.
        // Why 120 blocks?
        size_t blocks;
        if (next_free_block_index + 120 > next_free_block_index / 
                                            (2 * heap_residency))
            blocks = next_free_block_index + 120;
        else
            blocks = next_free_block_index / (2 * heap_residency);
//        ensure_space((blocks - next_free_block_index)*Block::size);
    }
    
    static void inline reclaim(Block* b) {
        // Do nothing   
    }
  
    /** Mark this object as grey, that is live, but not scanned */
    static inline GVMT_Object grey(Address obj) {
        return Memory::copy<SemiSpace>(obj);   
    }
    
    /** Return true if this object is grey or black */
    static inline bool is_live(Address a) {
        return Memory::forwarded(a);
    }
    
    static inline std::vector<Block*>::iterator begin_blocks() {
        return to_space->begin();
    }
    
    static inline std::vector<Block*>::iterator end_blocks() {
        std::vector<Block*>::iterator it = to_space->begin()+next_free_block_index; 
        assert(*it == (*to_space)[next_free_block_index]);
        return it;
    }
   
    // Stuff for pinning - Just abort.
    static void promote_pinned_block(Block* b) {
        abort();   
    }
    
    static void* pin(GVMT_Object obj) {
        abort();
    }
           
    static inline void sanity() { }
};

size_t SemiSpace::unused_memory;
std::vector<Block*> SemiSpace::semispace1;
std::vector<Block*> SemiSpace::semispace2;
//std::deque<Block*> SemiSpace::to_be_scanned;
std::vector<Block*>* SemiSpace::to_space;
std::vector<Block*>* SemiSpace::from_space;
size_t SemiSpace::next_free_block_index;
Address SemiSpace::copy_ptr;
Address SemiSpace::scan_ptr;
float SemiSpace::heap_residency;


#endif // GVMT_INTERNAL_SEMISPACE_H 

