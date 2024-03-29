#ifndef GVMT_INTERNAL_MEMORY_H 
#define GVMT_INTERNAL_MEMORY_H 

#include <assert.h>
#include <inttypes.h>
#define _XOPEN_SOURCE 600
#include <stdlib.h>
#include <malloc.h>
#include "gvmt/internal/gc.hpp"
#include "gvmt/internal/gc_templates.hpp"

#define LOG_CARD_SIZE 7
#define LOG_BLOCK_SIZE 14
#define LOG_ZONE_ALIGNMENT 19

#define LOG_CARDS_PER_BLOCK (LOG_BLOCK_SIZE - LOG_CARD_SIZE)

#define CARD_SIZE (1 << LOG_CARD_SIZE)
#define BLOCK_SIZE (1 << LOG_BLOCK_SIZE)
#define ZONE_ALIGNMENT (1 << LOG_ZONE_ALIGNMENT)
#define CARDS_PER_BLOCK (1 << LOG_CARDS_PER_BLOCK)
#define LARGE_OBJECT_SIZE (Block::size>>1)

#define WORD_SIZE sizeof(void*)
// This works for 16, 32 and 64, but not 128 bit words.
#define LOG_WORD_SIZE ((sizeof(void*)>>2)+1)
// This works for 32, 64 and 128 bit words, but not for 16 bit.
//#define LOG_WORD_SIZE ((sizeof(void*)>>3)+2)


#define BITS_PER_WORD (sizeof(void*)*8)

inline static bool crosses_power_of_2(char* ptr, size_t size, intptr_t power2) {
    size_t space = power2 - (((intptr_t)ptr) & (power2-1));
    return space < size;  
}

template <class C, uintptr_t log> class MemoryUnit {
    
public:
            
    static const uintptr_t log_size = log;
    static const uintptr_t size = 1 << log;
    
    static inline bool starts_at(Address a) {
        return (a.bits() & (C::size-1)) == 0; 
    }
    
    template <class T> static inline bool starts_at(T* ptr) {
        return (reinterpret_cast<uintptr_t>(ptr) & (C::size-1)) == 0; 
    }
    
    static inline bool crosses(Address a, uintptr_t size) {
        uintptr_t space = C::size - (a.bits() & (C::size-1));
        return space < size;  
    }
    
    static inline C* containing(Address ptr) {
        uintptr_t align = ptr.bits() & (-C::size);
        return reinterpret_cast<C*>(align);
    }
    
    template <class T> static inline C* containing(T* ptr) {
        uintptr_t align = reinterpret_cast<uintptr_t>(ptr) & (-C::size);
        return reinterpret_cast<C*>(align);
    }
    
    bool inline contains(Address ptr) {
        return containing(ptr) == this;
    }
    
    inline bool contains(Address ptr, uintptr_t size) {
        return contains(ptr) && !crosses(ptr, size);
    }
    
    template <class T> bool inline contains(T* ptr) {
        return containing(ptr) == this;
    }
    
    /** This returns the index of M containing mem, 
     *  within C */
    template <class M> static inline uintptr_t index_of(Address mem) {
        uintptr_t MASK = C::size - 1;
        return (mem.bits() & MASK) >> M::log_size;
    }
    
    /** This returns the index of M containing mem, 
     *  within C */
    template <class M> static inline uintptr_t index_of(M* mem) {
        uintptr_t MASK = C::size - 1;
        return (reinterpret_cast<uintptr_t>(mem) & MASK) >> M::log_size;
    }
    
    inline Address start() {
        return Address(reinterpret_cast<char*>(this));   
    }
    
    inline Address end() {
        return Address(reinterpret_cast<char*>(this)+size);
    }
    
    inline C* next() {
        return reinterpret_cast<C*>(reinterpret_cast<char*>(this)+size);
    }
    
    inline C* previous() {
        return reinterpret_cast<C*>(reinterpret_cast<char*>(this)-size);
    }
};

class Word {
    
public:
    
    static const uintptr_t log_size = (sizeof(void*) == 8) ? 3 : 2;
    static const uintptr_t size = sizeof(void*);

};

class EightWords {
    
public:
    
    static const uintptr_t log_size = 3+Word::log_size;
    static const uintptr_t size = 8*Word::size;   
};

// Why is this called Line not Card?
class Line : public MemoryUnit<Line, LOG_CARD_SIZE> {
    char pad[size];
};

class SpinLock {
    
    int mutex;
    
public:
    
    inline void lock() {
        while (!COMPARE_AND_SWAP(&mutex, 0, 1));  
    }
    
    inline void unlock() {
        mutex = 0;
    }
    
};

class Zone;

class Block : public MemoryUnit<Block, LOG_BLOCK_SIZE> {
    
    union {
        char pad[size];
        struct {
            Block* ring_next;
            Block* ring_previous;
        };
    };
        

public:   
    
    void clear_modified_map();
    
#ifdef NDEBUG    
    inline void verify() {}
#else
    void verify();
#endif

    static inline bool can_allocate(Address ptr, size_t obj_size) {
        uintptr_t space = (-((intptr_t)ptr.bits())) & (size-1);
        return obj_size <= space;
    }

    inline int8_t space();
    
    static inline int8_t space_of(Address a);
    
    inline void set_space(char s);
    
    inline bool is_pinned();
    
    inline void set_pinned(bool p);
    
    bool is_valid();
  
};


/** Handles virtual memory allocation */
class OS {
    
    static char* get_new_mmap_region(uintptr_t size);
    
public:
    
    static Zone* allocate_virtual_memory(size_t size);
    static void free_virtual_memory(Zone* zone, size_t size);
   
};

class Zone : public MemoryUnit<Zone, LOG_ZONE_ALIGNMENT> {
        
    void verify_header();
    void verify_layout();

    static inline uint8_t* mark_byte(Address mem) {
        // Each mark byte corresponds to 8 words = 32 bytes.
        Zone *z = containing(mem);
        uintptr_t index = index_of<EightWords>(mem);
        return &z->mark_map[index];
    }
    
public:
    
    static const uint32_t MAGIC_NUMBER = 
                          (0xFAB00000 | LOG_ZONE_ALIGNMENT);
    union {
        struct {
            union {
                bool permanent;
                struct {
                    char spaces[Zone::size/Block::size]; // 1 byte per block
                    uint32_t real_blocks;
                };
                struct {
                    uint8_t modified_map[Zone::size/Line::size];  
                    union {
                        uint8_t collector_line_data[Zone::size/Line::size]; 
                        uint32_t collector_block_data[Zone::size/Block::size];
                    };
                    union {
                        uint8_t pinned[Zone::size/Line::size]; 
                        uint8_t block_pinned[Zone::size/Block::size];
                    };
                };
                char pad[Block::size];   // align to block;
            };
            union {
                uint8_t mark_map[1];   // 1 bit per word
            };
        };
        Block blocks[1]; // Actual usable memory
    };
    
    static inline bool marked(Address mem) {
        assert(is_aligned(mem.bits()));
        uint8_t* byte = mark_byte(mem);
        size_t index = (mem.bits() >> Word::log_size) & 7;
        return (*byte >> index) & 1;
    }
    
    static inline void mark(Address mem) {
        assert(is_aligned(mem.bits()));
        uint8_t* byte = mark_byte(mem);
        size_t index = (mem.bits() >> Word::log_size) & 7;
        *byte |= 1<<index;
    }
    
    static inline void unmark(Address mem) {
        assert(is_aligned(mem.bits()));
        uint8_t* byte = mark_byte(mem);
        size_t index = (mem.bits() >> Word::log_size) & 7;
        *byte &= ~(1<<index);
    }
    
    /** Ensure object at mem is marked, return true if previously unmarked */
    static inline bool mark_if_unmarked(Address mem) {
        assert(is_aligned(mem.bits()));
        uint8_t* byte = mark_byte(mem);
        uint8_t bit = 1<<((mem.bits() >> Word::log_size) & 7);
        uint8_t read = *byte;
        if (read & bit) {
            return false;
        } else {
            *byte = read | bit;
            return true;
        }
    }
      
    static bool unmarked(Block* b) {
        int32_t* start = (int32_t*)mark_byte((char*)b);
        size_t i;
        for (i = 0; i < Block::size/Line::size; i++) {
            if (start[i])
                return false;
        }
        return true;
    }

    static void clear_mark_map(Block* b) {
        int32_t* start = (int32_t*)Zone::mark_byte((char*)b);
        size_t i;
        for (i = 0; i < Block::size/Line::size; i+= 2) {
            start[i] = 0;
            start[i+1] = 0;
        }
    }
    
    /** Returns the first usable Block, ie. Block index 2 */
    inline Block* first() {
        return reinterpret_cast<Block*>(this)->next()->next();
    }
    
    /** Returns the last real Block + 1, ie.
     * The first block not to have been fetched into real memory*/
    inline Block* first_virtual() {
        assert(this->real_blocks <= Zone::size/Block::size);
        return reinterpret_cast<Block*>(this) + this->real_blocks;
    }

    inline Block* end() {
        return reinterpret_cast<Block*>(reinterpret_cast<char*>(this)+size);
    }
    /** Does consistency checks */
    void verify();
    
    static void verify_heap();
    
    static void print_flags(Address addr);
    
    void print_blocks();
    
    /** Return true iff addr is a valid address in an allocated zone*/
    static bool valid_address(Address addr);
    
    inline uint8_t* modification_byte(Line* line) {
        uintptr_t index = index_of<Line>(line);
        return &this->modified_map[index];
    }
    
    inline bool modified(Line* line) {
        return *modification_byte(line) != 0;
    }
    
    inline void clear_modified(Line* line) {
        *modification_byte(line) = 0;
    }
    
    /** To do -- Replace magic numbers */
    template <class C> static inline void scan_marked_objects(Line* line) {
        Address obj = line->start();
        do {
            uint8_t mark_byte = *Zone::mark_byte(obj); 
            if (mark_byte) {
                for (int j = 0; j < 8; j++) {
                    if (mark_byte & (1 << j)) {
                        assert(Zone::marked(obj.plus_bytes(j*4)));
                        gc::scan_object<C>(obj.plus_bytes(j*4));
                    }
                }
            }
            obj = obj.plus_bytes(32);
        } while (obj < line->next()->start());
    }

};

inline int8_t Block::space_of(Address a) {
    Zone* z = Zone::containing(a);
    size_t index = Zone::index_of<Block>(a);
    return z->spaces[index];
}

inline int8_t Block::space() {
    return space_of(start());
} 

inline void Block::set_space(char s) {
    Zone* z = Zone::containing(this);
    size_t index = Zone::index_of<Block>(this);
    z->spaces[index] = s;
} 

    
inline bool Block::is_pinned() {
    Zone* z = Zone::containing(this);
    size_t index = Zone::index_of<Block>(this);
    return z->block_pinned[index];    
}
    
inline void Block::set_pinned(bool p) {
    Zone* z = Zone::containing(this);
    size_t index = Zone::index_of<Block>(this);
    z->block_pinned[index] = p;    
}

class Space {
    
    static const char* area_names[];
   
public:
    
    static const char* area_name(Address a);
    
    static const int FREE = 0;
    static const int NURSERY = -1;
    static const int PINNED = -2;

    // Since the block_info for very-large objects overlaps the card-marking table
    // LARGE must be the same as cards are marked with.
    static const int LARGE = 1; 
    static const int MATURE = 2;
    static const int INTERNAL = 3;
        
    static inline bool is_young(Address a) {
        return Block::space_of(a) < 0;
    }
   
};

class Heap {
 
    static std::vector<Zone*> zones;
    static size_t free_block_count;
    static std::vector<Block*> first_free_blocks;
    static Block* free_block_rings[ZONE_ALIGNMENT/BLOCK_SIZE];
    
    static void pop_out_of_ring(Block* b, size_t size) {
        assert(free_block_rings[size] != NULL);
        if (b == b->ring_next) {
            assert(free_block_rings[size] == b);
            free_block_rings[size] = NULL;
        } else {
            free_block_rings[size] = b->ring_next;
            Block *next, *previous;
            next = b->ring_next;
            previous = b->ring_previous;
            next->ring_previous = previous;
            previous->ring_next = next;
        }
    }
    
    static Block* allocate_from_zone(size_t count, int space) {
        for (size_t i = 0; i < first_free_blocks.size(); i++) {
            size_t end = Zone::index_of<Block>(first_free_blocks[i]) + count;
            if (end <= Zone::size/Block::size) {
                Block* result = first_free_blocks[i];
                first_free_blocks[i] += count;
                if (end == Zone::size/Block::size) {
                    first_free_blocks[i] = first_free_blocks[first_free_blocks.size()-1];
                    first_free_blocks.pop_back();
                }
                for (unsigned i = 0; i < count; i++) {
                    result[i].set_space(space);
                }                
                gvmt_real_heap_size += count * Block::size;
                free_block_count -= count;
                Zone::containing(result)->real_blocks += count;
                return result;
            }
        }
        return NULL;
    }

    static void add_new_zone() {
        Zone* z = OS::allocate_virtual_memory(Zone::size);
        zones.push_back(z);
        Block* first = z->first();
        first_free_blocks.push_back(first);
        // Account for header stuff - Will be real memory
        int wasted = Zone::index_of<Block>(first);
        z->real_blocks = wasted;
        gvmt_real_heap_size += wasted * Block::size;
        free_block_count += Zone::size/Block::size - wasted;
    }
    
    static void insert_in_ring(Block* b, size_t size) {
        assert(b >= Zone::containing(b)->first());
        Block* next = free_block_rings[size];
        if (next == NULL) {
            b->ring_previous = b;
            b->ring_next = b;
            free_block_rings[size] = b;
        } else {
            assert(next->space() == Space::FREE);
            Block *previous = next->ring_previous;
            assert(previous);
            assert(previous->space() == Space::FREE);
            b->ring_next = next;
            b->ring_previous = previous;
            previous->ring_next = b;
            next->ring_previous = b;
        }
    }

public:

    template <class Policy> static void init() {
        Block* b = (Block*)&gvmt_start_heap;
        Zone* z = Zone::containing(b);
        assert(b == (Block*)z);
        assert(b == Block::containing((char*)b));
        assert(sizeof(Block) == Block::size);
        while ((char*)z < &gvmt_end_heap) {
            gvmt_real_heap_size += Zone::size;
            gvmt_virtual_heap_size += Zone::size;
            zones.push_back(z);   
            z->real_blocks = Zone::size/Block::size;
            z = z->next();
        }
        // Skip zone headers
        if (b < Zone::containing(b)->first())
            b = Zone::containing(b)->first();
        Block* end = Block::containing(&gvmt_small_object_area_end);
        assert(end == (Block*)&gvmt_small_object_area_end);
        while (b < end) {
            Policy::data_block(b);
            b = b->next();
            // Skip zone headers
            if (b < Zone::containing(b)->first()) {
                b = Zone::containing(b)->first();
            }
        }
        b = Block::containing(&gvmt_small_object_area_end);
        end = Block::containing(&gvmt_large_object_area_start);
        assert(end == (Block*)&gvmt_large_object_area_start);
        init_free_blocks(b, end - b);
    }
    
    static void init_free_blocks(Block* b, size_t count) {
        if (count == 0)
            return;
        Block* end = b + count;
        while (Zone::containing(b)->first_virtual() < end) {
            insert_in_ring(b, Zone::containing(b)->first_virtual() - b);
            free_block_count += Zone::containing(b)->first_virtual() - b;
            Block* i;
            for (i = b; i < Zone::containing(b)->first_virtual(); i++) {
                assert(i->is_valid());
                assert(Zone::unmarked(i));
                i->set_space(Space::FREE);
            }   
            b = Zone::containing(b)->next()->first();
        }
        if (b < end) {
            insert_in_ring(b, end - b);
            free_block_count += end - b;
            Block* i;
            for (i = b; i < end; i++) {
                assert(i->is_valid());
                assert(Zone::unmarked(i));
                i->set_space(Space::FREE);
            }
        }
    }
    
    static inline void done_collection(void) {
        // Nothing to do.
    }
    
    static bool contains(Zone* z);
    
    static void ensure_space(size_t space) {
        while (space > available_space()) {
            add_new_zone();
        }
    }
    
    static size_t available_space() {
        return free_block_count * Block::size;
    }
    
    static inline Block* get_blocks_from_free_lists(int count, int space) {
        int size;
        assert(count < (int)(Zone::size/Block::size));
        for (size = count; free_block_rings[size] == NULL; size++) {
            if (size == Zone::size/Block::size-1)
                return NULL;
        }
        Block* result = free_block_rings[size];
        pop_out_of_ring(result, size);
        if (size > count) {
            Block* surplus = result + count;
            insert_in_ring(surplus, size-count);
        }
        free_block_count -= count;
        for (int i = 0; i < count; i++) {
            result[i].set_space(space);
        }                
        return result;
    }
     
    static Block* get_blocks(size_t count, int space, bool force) {
        assert(0 < count);
        assert(count < Zone::size/Block::size);
        assert(space != Space::FREE);
        Block* result = get_blocks_from_free_lists(count, space);
        if (result == NULL) {
            result = allocate_from_zone(count, space);
        }
        if (result == NULL && force) {
            add_new_zone();
            result = allocate_from_zone(count, space);
            assert(result != NULL);
        }
        return result;
    }
        
    static inline Block* get_block(int space, bool force) {
        Block* result = free_block_rings[1];
        if (result) {
            pop_out_of_ring(result, 1);
            free_block_count -= 1;
            assert(result->space() == Space::FREE);
            result->set_space(space);
            return result;
        }
        return get_blocks(1, space, force);
    }

    static void free_blocks(Block* blocks, size_t count) {
        Zone* z = Zone::containing(blocks);
        assert(z == Zone::containing((blocks + count-1)));
        assert(count > 0);
        Block* start, *end;
        start = blocks;
        while (start > z->first() && start->previous()->space() == Space::FREE) {
            start = start->previous(); 
        }
        end = blocks+count;
        while (end < z->first_virtual() && end->space() == Space::FREE) {
            end = end->next();
        }
        if (start != blocks) {
            pop_out_of_ring(start, blocks-start);
        }
        if (end != blocks+count) {
            pop_out_of_ring(blocks+count, end-blocks-count);
        }
        insert_in_ring(start, end-start);
        free_block_count += count;
        for (size_t i = 0; i < count; i++) {
            assert(Zone::unmarked(&blocks[i]));
            blocks[i].set_space(Space::FREE);
            blocks[i].set_pinned(false);
        }
    }
    
    class iterator {
        size_t index;
         
    public:
         
        iterator(size_t index) {
            this->index = index;
        }
         
        inline iterator& operator++ () {
            this->index++;
            return *this;
        }
        
        inline bool operator == (iterator other) {
            return index == other.index;
        }
        
        inline bool operator != (iterator other) {
            return index != other.index;
        }

        inline Zone*& operator*() {
            return zones[index];
        }

    };
        
    static inline iterator begin() {
        return iterator(0);
    }

    static inline iterator end() {
        return iterator(zones.size());
    }
    
    static void print_blocks();
    
};

namespace GC {
    
    extern std::vector<Block*> mark_stack_blocks;
    extern Address* mark_stack_pointer;
    extern Block* mark_stack_reserve;
    
    inline void push_mark_stack(Address addr) {
#ifndef NDEBUG
        int shape_buffer[GVMT_MAX_SHAPE_SIZE];
        assert(size_from_shape(gvmt_user_shape(addr.as_object(), shape_buffer)) == 
               gvmt_user_length(addr.as_object())); 
#endif
        if (Block::starts_at(mark_stack_pointer)) {
            mark_stack_blocks.push_back((Block*)mark_stack_pointer);
            if (mark_stack_reserve) {
                mark_stack_pointer = (Address*)mark_stack_reserve;
                mark_stack_reserve = 0;
            } else {
                Block* b = Heap::get_block(Space::INTERNAL, true);
                mark_stack_pointer = (Address*)b;
            }
        }
        *mark_stack_pointer = addr;
        mark_stack_pointer++;
    }
 
    inline bool mark_stack_is_empty() {
        return mark_stack_pointer == 0;
    }
    
    inline Address pop_mark_stack() {
        assert(!mark_stack_is_empty());
        mark_stack_pointer--;
        Address obj = *mark_stack_pointer;
        if (Block::starts_at(mark_stack_pointer)) {
            assert(Block::containing(mark_stack_pointer)->space() == Space::INTERNAL);
            if (mark_stack_reserve) {
                Heap::free_blocks(Block::containing(mark_stack_pointer), 1);
            } else {
                mark_stack_reserve = Block::containing(mark_stack_pointer);
            }
            Block* b = mark_stack_blocks.back();
            mark_stack_blocks.pop_back();
            mark_stack_pointer = ((Address*)b);
        }
        return obj;
    }
    
};

namespace gc {
    
    template <class Collection> void transitive_closure() {
        while (!GC::mark_stack_is_empty()) {
            Address obj = GC::pop_mark_stack();
            Address end = scan_object<Collection>(obj);
            Collection::scanned(obj, end);
        }
    }

};

class Memory {
    
    static inline GVMT_Object forwarding_address(Address p) {
        uintptr_t bits = p.read_word() & (~FORWARDING_BIT);
        return Address::from_bits(bits).as_object();
    } 
    
    static inline void set_forwarding_address(Address p, Address f) {
        p.write_word(f.bits() | FORWARDING_BIT);
    }
   
public:
 
    static inline int forwarded(Address p) {
        return p.read_word() & FORWARDING_BIT; 
    }
       
    static inline void move(Address from, Address to, size_t size) {
        assert(from.is_aligned());
        assert(to.is_aligned());
        assert(is_aligned(size));
        assert(!Block::containing(from)->is_pinned());
        assert(Block::containing(from)->is_valid());
        assert(Block::containing(to)->is_valid());
        Address to_ptr, from_ptr, end;
        to_ptr = to;
        from_ptr = from;
        end = to.plus_bytes(size);
        do {
            to_ptr.write_word(from_ptr.read_word());
            to_ptr = to_ptr.next_word();
            from_ptr = from_ptr.next_word();
        } while (to_ptr < end);
    }
    
    template <class Policy> static inline GVMT_Object copy(Address a) {
        if (forwarded(a)) {
            return forwarding_address(a);
        }
        size_t size = align(gvmt_user_length(a.as_object()));
        Address result = Policy::allocate(size);
        move(a, result, size);
        set_forwarding_address(a, result);
        Zone::mark(result);
        GC::push_mark_stack(result);
        return result.as_object();
    }
    
    
};

#endif // GVMT_INTERNAL_MEMORY_H 
