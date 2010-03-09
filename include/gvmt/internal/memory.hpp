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
#define LOG_SUPER_BLOCK_ALIGNMENT 19

#define LOG_CARDS_PER_BLOCK (LOG_BLOCK_SIZE - LOG_CARD_SIZE)

#define CARD_SIZE (1 << LOG_CARD_SIZE)
#define BLOCK_SIZE (1 << LOG_BLOCK_SIZE)
#define SUPER_BLOCK_ALIGNMENT (1 << LOG_SUPER_BLOCK_ALIGNMENT)
#define CARDS_PER_BLOCK (1 << LOG_CARDS_PER_BLOCK)
#define LARGE_OBJECT_SIZE (1 << 11)

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

class Line : public MemoryUnit<Line, 7> {
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

class SuperBlock;

class Block : public MemoryUnit<Block, 14> {
    
    char pad[size];

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

    inline char space();
    
    static inline char space_of(Address a);
    
    inline void set_space(char s);
  
};


/** This object is a SuperBlock* but unitialised, so that it will not require
 * any real memory until it is activated */
class ReservedMemoryHandle {
    
    uintptr_t data;
    
    static char* get_new_mmap_region(uintptr_t size);
    
public:
    static ReservedMemoryHandle allocate(size_t size);
    
    SuperBlock* activate();
    
    inline bool valid() {
        return data != 0;   
    }
   
};

class SuperBlock : public MemoryUnit<SuperBlock, 19> {
        
    void verify_header();
    void verify_layout();
    
public:
    
    static const uint32_t MAGIC_NUMBER = 
                          (0xFAB00000 | LOG_SUPER_BLOCK_ALIGNMENT);
    union {
        struct {
            union {
                bool permanent;
                char spaces[1]; // 1 byte per block
                struct {
                    uint8_t modified_map[SuperBlock::size/Line::size];  
                    union {
                        uint8_t collector_line_data[SuperBlock::size/Line::size]; 
                        uint32_t collector_block_data[SuperBlock::size/Block::size];
                    };
                    uint8_t pinned[SuperBlock::size/Line::size]; 
                };
                char pad[Block::size];   // align to block;
            };
            union {
                uint8_t mark_map[1];   // 1 bit per word
            };
        };
        Block blocks[1]; // Actual usable memory
    };
    
    static SuperBlock* allocate(size_t size) {
        ReservedMemoryHandle handle = ReservedMemoryHandle::allocate(size);
        if (!handle.valid())
            return NULL;
        return handle.activate();
    }

    static inline uint8_t* mark_byte(Address mem) {
        // Each mark byte corresponds to 8 words = 32 bytes.
        SuperBlock *sb = containing(mem);
        uintptr_t index = index_of<EightWords>(mem);
        return &sb->mark_map[index];
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
    
    inline Block* first() {
        return reinterpret_cast<Block*>(this);
    }
    
    inline Block* end() {
        return reinterpret_cast<Block*>(reinterpret_cast<char*>(this)+size);
    }
    /** Does consistency checks */
    void verify();
    
    static void verify_heap();
    
    static void print_flags(Address addr);
    
    /** Return true iff addr is a valid address in an allocated super-block*/
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

inline char Block::space_of(Address a) {
    SuperBlock* sb = SuperBlock::containing(a);
    size_t index = SuperBlock::index_of<Block>(a);
    return sb->spaces[index];
}

inline char Block::space() {
    return space_of(start());
} 

inline void Block::set_space(char s) {
    SuperBlock* sb = SuperBlock::containing(this);
    size_t index = SuperBlock::index_of<Block>(this);
    sb->spaces[index] = s;
} 

class Space {
    
    static const char* area_names[];
   
public:
    
    static const char* area_name(Address a);
    
    static const int MATURE = 0;
    static const int NURSERY = -1;
    static const int PINNED = -2;

    // Since the block_info for very-large objects overlaps the card-marking table
    // LARGE must be the same as cards are marked with.
    static const int LARGE = 1; 
        
    static inline bool is_young(GVMT_Object p) {
        return Block::space_of(p) < MATURE;
    }
        
    static inline bool is_young(Address a) {
        return Block::space_of(a) < MATURE;
    }
   
};

class Memory {
    
    static inline GVMT_Object forwarding_address(GVMT_Object p) {
        uintptr_t bits = gc::untag(p).read_word() & (~FORWARDING_BIT);
        Address r  = Address::from_bits(bits);
        return gc::tag(r, gc::get_tag(p));
    } 
    
    static inline void set_forwarding_address(GVMT_Object p, Address f) {
        gc::untag(p).write_word(f.bits() | FORWARDING_BIT);
    }
   
public:
 
    static inline int forwarded(GVMT_Object p) {
        return gc::untag(p).read_word() & FORWARDING_BIT; 
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
    
    template <class Policy> static inline GVMT_Object copy(GVMT_Object p) {
        if (forwarded(p)) {
            return forwarding_address(p);
        }
#ifndef NDEBUG
        int shape_buffer[GVMT_MAX_SHAPE_SIZE];
        assert(*(intptr_t*)p);
        assert(size_from_shape(gvmt_shape(p, shape_buffer)) == gvmt_length(p));
#endif
        size_t size = align(gvmt_length(p));
        Address result = Policy::allocate(size);
        move(gc::untag(p), result, size);
        set_forwarding_address(p, result);
        SuperBlock::mark(result);
        GC::push_mark_stack(result);
        return gc::tag(result, gc::get_tag(p));
    }

    template <class Policy> static void heap_init() {
        Block* b = (Block*)&gvmt_start_heap;
        assert(b == Block::containing((char*)b));
        // Skip super-block headers
        while (SuperBlock::index_of<Block>((char*)b) < 2)
            b = b->next();
        uint8_t area;
        while ((area = b->space()) == Space::MATURE) {
            if (((intptr_t*)b)[0] != 0) {
                Policy::data_block(b);
            }
            b = b->next();
            // Skip super-block headers
            if (SuperBlock::index_of<Block>((char*)b) == 0) {
                b = b->next(); 
                b = b->next();
            }
        }
    }
    
    
};

template <int area> class BlockManager {
 
    std::vector<SuperBlock*> super_blocks;
    std::vector<uint32_t>free_bits;
    
    inline Block* allocate_from_super_block(size_t index, size_t count) {
        if (free_bits[index] == 0)
            return NULL;
        size_t i, mask = (1<<count) - 1;
        for(i = 2; i <= SuperBlock::size/Block::size-count; i++) {
            if ((free_bits[index] & (mask<<i)) == (mask<<i)) {
                free_bits[index] &= (~(mask<< i));
                Block* result = &super_blocks[index]->blocks[i];
                for (unsigned i = 0; i < count; i++) {
                    result[i].set_space(area);
                }
                return result;
            }
        }
        return NULL;
    }
    
    void new_super_block() {
        super_blocks.push_back(SuperBlock::allocate(SuperBlock::size));   
        free_bits.push_back(-4);
    }
    
public:
    
    Block* allocate_blocks(size_t count) {
        assert(0 < count);
        assert(count < SuperBlock::size/Block::size);
        Block* result;
        size_t index = 0;
        while (index < super_blocks.size()) {
            result = allocate_from_super_block(index, count);
            if (result) {
                assert(SuperBlock::index_of<Block>(result) >= 2);
                assert(SuperBlock::index_of<Block>(result)+count <= SuperBlock::size/Block::size);
                return result;
            }
            index++;
        }
        new_super_block();
        result = allocate_from_super_block(index, count);
        assert (result != NULL);
        return result;
    }

    void free_blocks(Block* blocks, size_t count) {
        SuperBlock* sb = SuperBlock::containing(blocks);
        assert(sb == SuperBlock::containing((blocks + count)));
        size_t i, end, mask;
        for (i = 0, end = super_blocks.size(); i < end; i++) {
            if (sb == super_blocks[i]) {
                mask = (1<<count) - 1;
                free_bits[i] |= (mask<<SuperBlock::index_of<Block>(blocks));
                return;
            }
        }
        assert("Cannot reach here" && false);
    }
        
    inline Block* allocate_block() {
        return allocate_blocks(1);
    }
    
};

extern "C" {
    
    extern Block gvmt_large_object_area_start;

}


#endif // GVMT_INTERNAL_MEMORY_H 
