#ifndef GVMT_INTERNAL_MEMORY_H 
#define GVMT_INTERNAL_MEMORY_H 

#include <assert.h>
#include <inttypes.h>
#define _XOPEN_SOURCE 600
#include <stdlib.h>
#include <malloc.h>
#include "gvmt/internal/gc.hpp"

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
            Block* linkedlist_next;
            Block* linkedlist_previous;
        };
    };
        

public:   
    
    void clear_modified_map();

    
    void clear_mark_map();   
    
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
  
};


/** Handles virtual memory allocation */
class OS {
    
    static uintptr_t virtual_memory_size;
    static char* get_new_mmap_region(uintptr_t size);
    
public:
    
    static Zone* allocate_virtual_memory(size_t size);
    static void free_virtual_memory(Zone* zone, size_t size);
    
    static uintptr_t virtual_memory_allocated();
   
};

class Zone : public MemoryUnit<Zone, LOG_ZONE_ALIGNMENT> {
        
    void verify_header();
    void verify_layout();
    
public:
    
    static const uint32_t MAGIC_NUMBER = 
                          (0xFAB00000 | LOG_ZONE_ALIGNMENT);
    union {
        struct {
            union {
                bool permanent;
                struct {
                    char spaces[Zone::size/Block::size]; // 1 byte per block
                    uint32_t index;
                };
                struct {
                    uint8_t modified_map[Zone::size/Line::size];  
                    union {
                        uint8_t collector_line_data[Zone::size/Line::size]; 
                        uint32_t collector_block_data[Zone::size/Block::size];
                    };
                    uint8_t pinned[Zone::size/Line::size]; 
                };
                char pad[Block::size];   // align to block;
            };
            union {
                uint8_t mark_map[1];   // 1 bit per word
            };
        };
        Block blocks[1]; // Actual usable memory
    };

    static inline uint8_t* mark_byte(Address mem) {
        // Each mark byte corresponds to 8 words = 32 bytes.
        Zone *z = containing(mem);
        uintptr_t index = index_of<EightWords>(mem);
        return &z->mark_map[index];
    }
    
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
    
    /** Returns the first usual Block, ie. Block index 2 */
    inline Block* first() {
        return reinterpret_cast<Block*>(this)->next()->next();
    }
    
    inline Block* end() {
        return reinterpret_cast<Block*>(reinterpret_cast<char*>(this)+size);
    }
    /** Does consistency checks */
    void verify();
    
    static void verify_heap();
    
    static void print_flags(Address addr);
    
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
        
    static inline bool is_young(Address a) {
        return Block::space_of(a) < 0;
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

class Heap {
 
    static std::vector<Zone*> zones;
    static std::vector<uint32_t>free_bits;
    static int first_free;
    static int virtual_zones;
    static size_t free_block_count;
    static size_t single_block_cursor;
    static size_t multi_block_cursor;
    
    static Block* allocate_from_zone(size_t index, size_t count, int space) {
        if (free_bits[index] == 0)
            return NULL;
        size_t i, mask = (1<<count) - 1;
        for(i = 2; i <= Zone::size/Block::size-count; i++) {
            if ((free_bits[index] & (mask<<i)) == (mask<<i)) {
                free_bits[index] &= (~(mask<< i));
                Block* result = &zones[index]->blocks[i];
                assert(Zone::index_of<Block>(result) == i);
                for (unsigned i = 0; i < count; i++) {
                    result[i].set_space(space);
                }
                free_block_count -= count;
                return result;
            }
        }
        return NULL;
    }

    static inline Block* allocate_from_new_zone(size_t count, int space, bool force) {
        if (force || virtual_zones > 0) {
            Zone* z = OS::allocate_virtual_memory(Zone::size);
            z->index = zones.size();
            zones.push_back(z);   
            free_bits.push_back(-4);
            free_block_count += Zone::size/Block::size-2;
            virtual_zones--;
            multi_block_cursor = z->index;
            Block* result = allocate_from_zone(z->index, count, space);
            assert (result != NULL);
            return result;
        } else {
            return NULL;
        }
    }
    
public:

    template <class Policy> static void init() {
        Block* b = (Block*)&gvmt_start_heap;
        Zone* z = Zone::containing(b);
        assert(b == (Block*)z);
        assert(b == Block::containing((char*)b));
        assert(sizeof(Block) == Block::size);
        // Skip zone headers
        while (Zone::index_of<Block>((char*)b) < 2)
            b = b->next();
        uint8_t area;
        while ((area = b->space()) == Space::MATURE) {
            if (((intptr_t*)b)[0] != 0) {
                Policy::data_block(b);
            }
            b = b->next();
            // Skip zone headers
            if (Zone::index_of<Block>((char*)b) == 0) {
                b = b->next(); 
                b = b->next();
            }
        }
        while ((char*)z < &gvmt_end_heap) {
            z->index = zones.size();
            zones.push_back(z);   
            free_bits.push_back(0);
            z = z->next();
        }
    }
    
    static inline void done_collection(void) {
        // reset cursors to start
        single_block_cursor = 0;
        multi_block_cursor = 0;
    }
    
    static void ensure_space(size_t space) {
        int extra = space - available_space();
        if (extra > 0)
            virtual_zones += (extra+Zone::size-1)/Zone::size;
    }
    
    static int available_space() {
        // Computed space may be negative from previous forced allocation.
        int space = free_block_count * Block::size + 
               virtual_zones * Zone::size;
        return space < 0 ? 0 : space;
    }
     
    static Block* get_blocks(size_t count, int space, bool force) {
        assert(0 < count);
        assert(count < Zone::size/Block::size);
        assert(space != Space::FREE);
        if (count == 1)
            return get_block(space, force);
        Block* result;
        while (multi_block_cursor < zones.size()) {
            if (free_bits[multi_block_cursor]) {
                result = allocate_from_zone(multi_block_cursor, count, space);
                if (result) {
                    assert(Zone::index_of<Block>(result) >= 2);
                    assert(Zone::index_of<Block>(result)+count <= 
                           Zone::size/Block::size);
                    return result;
                }
            }
            multi_block_cursor++;
        }
        return allocate_from_new_zone(count, space, force);
    }
        
    static inline Block* get_block(int space, bool force) {
        while (single_block_cursor < zones.size()) {
            if (free_bits[single_block_cursor]) {
                Block* result = allocate_from_zone(single_block_cursor, 1, space);
                assert(result);
                return result;
            }
            single_block_cursor++;
        }
        return allocate_from_new_zone(1, space, force);
    }

    static void free_blocks(Block* blocks, size_t count) {
        Zone* z = Zone::containing(blocks);
        assert(z == Zone::containing((blocks + count-1)));
        assert(z->index < zones.size());
        assert(count > 0);
        uint32_t mask = (1<<count) - 1;
        uint32_t to_free = (mask<<Zone::index_of<Block>(blocks));
        assert ((free_bits[z->index] & to_free) == 0);
        free_bits[z->index] |= to_free;
        free_block_count += count;
        for (size_t i = 0; i < count; i++) {
            blocks[i].set_space(Space::FREE);
        }
    }
    
    class iterator {
        size_t index;
         
    public:
         
        iterator(size_t index) {
            this->index = index;
        }
         
        inline iterator& operator++ () {
            index++;
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
    
};
 
extern "C" {
    
    extern Block gvmt_large_object_area_start;

}

#endif // GVMT_INTERNAL_MEMORY_H 
