#ifndef GVMT_INTERNAL_MARK_SWEEP_H 
#define GVMT_INTERNAL_MARK_SWEEP_H 

#include "gvmt/internal/memory.hpp"

// This class is not thread-safe and must be locked externally.
class MarkSweepList {
    
    size_t size;
    std::deque<Block*> blocks;
    GVMT_Object free_list;
    
    template <class Collection> void process_old_young(Block* b) {
        Zone* z = Zone::containing(b);
        assert(b->space() == Space::LARGE);
        Address ptr = b->start();
        do {
            if (Zone::marked(ptr)) {
                Line* line = Line::containing(ptr);
                if (z->modified(line)) {
                    gc::scan_object<Collection>(ptr);
                    z->clear_modified(line);
                }
            }
            ptr = ptr.plus_bytes(size);
        } while (b->contains(ptr, size));
    }
    
    /** Return true if live */
    bool inline sweep_block(Block* b) {
        Address ptr = b->start();
        int marked = 0;
        do {
            if (Zone::marked(ptr)) {
                marked++;
            } 
            ptr = ptr.plus_bytes(size);
        } while (b->contains(ptr, size));
        if (marked == 0) {
            Heap::free_blocks(b, 1);
            return false;
        }
        ptr = b->start();
        do {
            if (!Zone::marked(ptr)) {
                ptr.write_word(reinterpret_cast<uintptr_t>(free_list));
                free_list = ptr.as_object();
            } 
            ptr = ptr.plus_bytes(size);
        } while (b->contains(ptr, size));
        return true;
    }
    
    void clear_marks(Block* b) {
        Address ptr = b->start();
        do {
            Zone::unmark(ptr);
            ptr = ptr.plus_bytes(size);
        } while (b->contains(ptr, size));
    }
        
public:
    
    void verify() {
        for (std::deque<Block*>::iterator it = blocks.begin(); it != blocks.end(); it++) {
            Block* b = *it;
            assert(b->space() == Space::LARGE);
            Address ptr = b->start();
            do {
                if (ptr.read_word() == 0)
                    break;
                assert(gvmt_user_length(ptr.as_object()) <= size);
                ptr = ptr.plus_bytes(size);
            } while (b->contains(ptr, size));
        }
    }
    
    inline void add_block(Block* b) {
        assert(Block::containing(b) == b);
        Address ptr = b->start();
        do {
            ptr.write_word(reinterpret_cast<uintptr_t>(free_list));
            free_list = ptr.as_object();
            ptr = ptr.plus_bytes(size);
        } while (b->contains(ptr, size));
        blocks.push_back(b);
    }   
    
    inline bool can_allocate(size_t size) {
        return size <= this->size;
    }
        
    void init(size_t size) {
        this->size = size;
    }
    
    void pre_collection() {
        free_list = NULL;
        for (std::deque<Block*>::iterator it = blocks.begin(); it != blocks.end(); it++) {
            clear_marks(*it);
        }
    }
    
    inline GVMT_Object allocate() {
        GVMT_Object result = free_list;
        if (result == NULL)
            return NULL;
        free_list = *reinterpret_cast<GVMT_Object*>(result);
        Zone::mark(Address(result));
        return result;
    }

    void sweep() {
        for(int i = 0, n = blocks.size(); i < n; i++) {
            Block* b = blocks.back();
            blocks.pop_back();
            if (sweep_block(b))
                blocks.push_front(b);
        }
    }
    
    void init_block(Block* b) {
        assert(b->is_valid());
        blocks.push_back(b);
        Address ptr = b->start();
        assert(ptr.read_word() != 0);
        do {
            assert(Zone::marked(ptr));
            ptr = ptr.plus_bytes(size);
        } while (ptr.read_word() != 0 && !Block::crosses(ptr, size));
        b->set_space(Space::LARGE);
    }
    
    template <class Action> void process_old_young() {
#ifndef NDEBUG
        verify();
#endif
        for (std::deque<Block*>::iterator it = blocks.begin(); it != blocks.end(); it++) {
            process_old_young<Action>(*it);
        }
    }   
    
};

    
#endif // GVMT_INTERNAL_MARK_SWEEP_H 
