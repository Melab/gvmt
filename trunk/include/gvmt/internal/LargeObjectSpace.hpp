#ifndef GVMT_INTERNAL_LARGE_OBJECT_SPACE_H 
#define GVMT_INTERNAL_LARGE_OBJECT_SPACE_H 

#include "gvmt/internal/memory.hpp"
#include "gvmt/internal/MarkSweep.hpp"

struct BigObject {
    BigObject* next;
    char object;
};


class LargeObjectSpace: Space {
    
    static LargeObjectMarkSweepList lists[6];
    static BlockManager<Space::LARGE> manager;
    static BigObject big_objects;
    static bool initialised;
    
    static inline size_t blocks_for_big_object(size_t size) {
        return (size+sizeof(void*)+(Block::size-1))>>Block::log_size; 
    }
    
    static GVMT_Object allocate_very_large_object(size_t size) {
        assert("To do" && false);
        abort();
    }
    
    static void sweep_big_objects() {
        BigObject* prev = &big_objects;
        BigObject* obj = prev->next;
        while (obj) {
            if (SuperBlock::marked(&obj->object)) {
                *SuperBlock::mark_byte(&obj->object) = 0;
                assert(!SuperBlock::marked(&obj->object));
                prev = obj;
            } else {
                prev->next = obj->next;
                char* c = &obj->object;
                int len = align(gvmt_length(reinterpret_cast<GVMT_Object>(c)));
                size_t blocks = blocks_for_big_object(len);
                assert(Block::containing((char*)obj) == (Block*)obj);
                manager.free_blocks((Block*)obj, blocks);
            }
            obj = obj->next;
        }
    }
    
public:
           
    static inline bool in(GVMT_Object p) {
        return Block::space_of(p) > 0;
    }
           
    static inline bool in(Address a) {
        return Block::space_of(a) > 0;
    }
    
    static void init() {
        lists[0].init(2048);
        lists[1].init(2720);
        lists[2].init(4096);
        lists[3].init(5456);
        lists[4].init(8192);
        lists[5].init(10912);
        Block* b = &gvmt_large_object_area_start;
        assert(b == Block::containing((char*)b));
        // Skip super-block headers
        while (SuperBlock::index_of<Block>((char*)b) < 2)
            b++;
        int8_t area;
        while ((area = b->space()) >= 0) {
            if (area != 0) {
                assert(area >= 22);
                assert(area <= 27);
                lists[area-22].init_block(b);
                b->set_space(Space::LARGE);
            }
            b++;
            // Skip super-block headers
            if (SuperBlock::index_of<Block>((char*)b) == 0)
                b += 2;
        }
        if (area == -128) {
            big_objects.next = reinterpret_cast<BigObject*>(b);
        } else {
            assert(area == -127);
            big_objects.next = NULL;
        }
        while (b < (Block*)&gvmt_end_heap) {
            b->set_space(Space::LARGE);
            b++;
        }
        initialised = true;
    }
    
    static GVMT_Object allocate(size_t size) {
        assert(initialised);
        // Space for mark word.
        if (size >= SUPER_BLOCK_ALIGNMENT/2)
            return allocate_very_large_object(size);
        size_t index;
        for (index = 0; index < 6; index++) {
            if (lists[index].can_allocate(size)) {
                LargeObjectMarkSweepList* list = &lists[index];
                GVMT_Object result = list->allocate();
                if (result == NULL) { // List full
                    list->add_block(manager.allocate_block());
                    result = list->allocate();
                }
                return result;
            }  
        }
        size_t blocks = blocks_for_big_object(size);
        BigObject* ptr = (BigObject*)manager.allocate_blocks(blocks);
        ptr->next = big_objects.next;
        big_objects.next = ptr;
        char* c = &ptr->object;
        return reinterpret_cast<GVMT_Object>(c);
    }
    
    static inline GVMT_Object grey(GVMT_Object obj) {
        Address addr = gc::untag(obj);
        assert(in(addr));
        if (!SuperBlock::marked(addr)) {
            SuperBlock::mark(addr);
            assert(SuperBlock::marked(addr));
            GC::push_mark_stack(obj);
        }
        return obj;
    }
    
    static inline bool is_live(GVMT_Object obj) {
        Address addr = gc::untag(obj);
        return SuperBlock::marked(addr);
    }
    
    static void sweep() {
        assert(initialised);
        for (int i = 0; i < 6; i++)
            lists[i].sweep();
        sweep_big_objects();
    }

    template <class Collection> static void process_old_young() {
        for (int i = 0; i < 6; i++)
            lists[i].process_old_young<Collection>();
        BigObject* obj = big_objects.next;
        while (obj) {
            char* object = &obj->object;
            SuperBlock* sb = SuperBlock::containing(object);
            Line* line = Line::containing(object);
            if (sb->modified(line)) {
                gc::scan_object<Collection>(object);
                sb->clear_modified(line);
            }
            obj = obj->next;
        }
    }

    
};

LargeObjectMarkSweepList LargeObjectSpace::lists[6];
BlockManager<Space::LARGE> LargeObjectSpace::manager;
BigObject LargeObjectSpace::big_objects;
bool LargeObjectSpace::initialised = false;


#endif // GVMT_INTERNAL_LARGE_OBJECT_SPACE_H 
