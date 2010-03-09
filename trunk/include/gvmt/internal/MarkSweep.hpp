#ifndef GVMT_INTERNAL_MARK_SWEEP_H 
#define GVMT_INTERNAL_MARK_SWEEP_H 

#include "gvmt/internal/memory.hpp"

// This class is not thread-safe and must be locked externally.
class LargeObjectMarkSweepList {
    std::vector<Block*> blocks;
    std::vector<int32_t>free;
    size_t object_spacing;
    
    template <class Collection> void process_old_young(Block* b, int& free_it) {
        SuperBlock* sb = SuperBlock::containing(b);
        assert(b->space() == Space::LARGE);
        Address ptr = b->start();
        int index = 0;
        do {
            if ((free_it & (1 << index)) == 0) {
                Line* line = Line::containing(ptr);
                if (sb->modified(line)) {
                    gc::scan_object<Collection>(ptr);
                    sb->clear_modified(line);
                }
            }
            ptr = ptr.plus_bytes(object_spacing);
            index++;
            assert(index < 32);
        } while (b->contains(ptr, object_spacing));
    }
        
    void inline sweep_block(Block* b, int& free_it) {
        assert(b->space() == Space::LARGE);
        Address ptr = b->start();
        int index = 0;
        do {
            if (SuperBlock::marked(ptr)) {
                assert(object_spacing >= 8 * WORD_SIZE);
                *SuperBlock::mark_byte(ptr) = 0;
                assert(!SuperBlock::marked(ptr));
            } else {
                free_it = free_it | (1 << index);
            }
            ptr = ptr.plus_bytes(object_spacing);
            index++;
            assert(index < 32);
        } while (b->contains(ptr, object_spacing));
    }

public:
    
    inline bool can_allocate(size_t size) {
        return size <= object_spacing;
    }
    
    inline void add_block(Block* b) {
        Address ptr = b->start();
        int index = 0;
        do {
            ptr = ptr.plus_bytes(object_spacing);
            index++;
            assert(index < 32);
        } while (b->contains(ptr, object_spacing));
        blocks.push_back(b);
        free.push_back((1 << index)-1);
    }
    
    inline GVMT_Object allocate_from_block(Block* b, int& free_it) {
        assert(b->space() == Space::LARGE);
        Address ptr = b->start();
        size_t index = 0;
        while ((free_it & (1 << index)) == 0) {
            ptr = ptr.plus_bytes(object_spacing);
            index++;
            assert(index < 32);
        }
        free_it = free_it & (~(1 << index));
        // Mark all modification bytes.
        SuperBlock *sb = SuperBlock::containing(ptr);
        uint8_t* mod = sb->modification_byte(Line::containing(ptr));
        for(index = 0; index < (object_spacing / sizeof(Line)); index++) {
            mod[index] = 1;
        }
        return ptr.as_object();
    }
    
    inline GVMT_Object allocate() {
        std::vector<Block*>::iterator block_it;
        std::vector<int32_t>::iterator free_it = free.begin();
        for (block_it = blocks.begin(); block_it != blocks.end(); block_it++) {
            if (*free_it) {
                return allocate_from_block(*block_it, *free_it);
            }
            free_it++;
        }
        return NULL;
    }

    void sweep() {
        std::vector<Block*>::iterator block_it;
        std::vector<int32_t>::iterator free_it = free.begin();
        for (block_it = blocks.begin(); block_it != blocks.end(); block_it++) {
            sweep_block(*block_it, *free_it);
            free_it++;
        }
    }
    
    void init_block(Block* b) {
        blocks.push_back(b);
        Address ptr = b->start();
        int index = 0;
        assert(ptr.read_word() != 0);
        do {
            ptr = ptr.plus_bytes(object_spacing);
            index++;
            assert(index < 32);
        } while (ptr.read_word() != 0 && !Block::crosses(ptr, object_spacing));
        int per_block = sizeof(Block)/object_spacing;
        free.push_back((1 << per_block)-(1 << index));
    }
        
    void init(size_t object_spacing) {
        this->object_spacing = object_spacing;
    }
    
    template <class Action> void process_old_young() {
        std::vector<Block*>::iterator it;
        std::vector<int32_t>::iterator free_it = free.begin();
        for (it = blocks.begin(); it != blocks.end(); it++) {
            process_old_young<Action>(*it, *free_it);
            free_it++;
        }
    }   
 
};

class LazyMarkSweepList {
    std::vector<Block*> blocks;
    char* free;
    char* swept;
    size_t block_index;

};
    
    
#endif // GVMT_INTERNAL_MARK_SWEEP_H 
