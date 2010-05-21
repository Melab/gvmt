#ifndef GVMT_INTERNAL_LARGE_OBJECT_SPACE_H 
#define GVMT_INTERNAL_LARGE_OBJECT_SPACE_H 

#include "gvmt/internal/memory.hpp"
#include "gvmt/internal/MarkSweep.hpp"

/** The Large Object Space is divided into 7 subspaces.
 * There are 6 Medium sized object lists from 2-3k to 11-14k.
 * One list for larger objects from 14k to 256k and 
 * a VeryLargeObject area for objects > 256k.
 * The medium sized objects are of sub-block size and the lists 
 * track and free blocks.
 * The larger objects require at least one block each and grab blocks directly.
 * Very large objects require at least one zone each and get their memory
 * directly from the OS.
 *
 */

struct BigObject {
    BigObject* next;
    char object;
};

struct VeryLargeObject {
    union {
        struct {
            char pad1[Block::size];
            VeryLargeObject* next;
            size_t size;
        };
        char pad[Block::size*2];
    };
    char object;
};


class LargeObjectSpace: Space {
    
    static MarkSweepList lists[6];
    static BigObject big_objects;
    static VeryLargeObject* very_large_objects;
    static bool initialised;
    
    static inline size_t blocks_for_big_object(size_t size) {
        return (size+sizeof(void*)+(Block::size-1))>>Block::log_size; 
    }
    
    static GVMT_Object allocate_very_large_object(size_t size) {
        assert(size >= Zone::size/2);
        assert (size < (1 << 29));
        size += Block::size*2;
        VeryLargeObject* vlo = (VeryLargeObject*)Zone::allocate(size);
        vlo->size = size;
        vlo->next = very_large_objects;
        very_large_objects = vlo;
        char* start = &vlo->object;
        Block::containing(start)->set_space(Space::LARGE);
        return reinterpret_cast<GVMT_Object>(start);
    }
    
    static GVMT_Object allocate_big_object(size_t size) {
        size_t blocks = blocks_for_big_object(size);
        BigObject* ptr = (BigObject*)Heap::get_blocks(blocks, Space::LARGE, false);
        if (ptr == NULL)
            return NULL;
        ptr->next = big_objects.next;
        big_objects.next = ptr;
        char* c = &ptr->object;
        return reinterpret_cast<GVMT_Object>(c);
    }
    
    static void sweep_big_objects() {
        BigObject* prev = &big_objects;
        BigObject* obj = prev->next;
        while (obj) {
            if (Zone::marked(&obj->object)) {
                *Zone::mark_byte(&obj->object) = 0;
                assert(!Zone::marked(&obj->object));
                prev = obj;
            } else {
                prev->next = obj->next;
                char* c = &obj->object;
                int len = align(gvmt_length(reinterpret_cast<GVMT_Object>(c)));
                size_t blocks = blocks_for_big_object(len);
                assert(Block::containing((char*)obj) == (Block*)obj);
                Heap::free_blocks((Block*)obj, blocks);
            }
            obj = obj->next;
        }
    }
    
    static void sweep_very_large_objects() {
        VeryLargeObject* prev = NULL;
        VeryLargeObject* obj = very_large_objects;
        VeryLargeObject* next;
        while (obj) {
            if (Zone::marked(&obj->object)) {
                *Zone::mark_byte(&obj->object) = 0;
                assert(!Zone::marked(&obj->object));
                prev = obj;
                next = obj->next;
            } else {
                if (prev == NULL) 
                    very_large_objects = obj->next;
                else
                    prev->next = obj->next;
                size_t size = obj->size;
                size = (size + (Zone::size - 1)) & -Zone::size;
                next = obj->next;
                munmap(obj, size);
            }
            obj = next;
        }
    }
    
public:
           
    static inline bool in(GVMT_Object p) {
        return Block::space_of(p) == Space::LARGE;
    }
           
    static inline bool in(Address a) {
        return Block::space_of(a) == Space::LARGE;
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
        // Skip zone headers
        while (Zone::index_of<Block>((char*)b) < 2)
            b++;
        int8_t area;
        while ((area = b->space()) >= 0) {
            if (area != 0) {
                assert(area >= 22);
                assert(area <= 27);
                lists[area-22].init_block(b);
            }
            b++;
            // Skip zone headers
            if (Zone::index_of<Block>((char*)b) == 0)
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
        very_large_objects = NULL;
        initialised = true;
    }
    
    static GVMT_Object allocate(size_t size, bool force) {
        assert(initialised);
        // Space for mark word.
        if (size >= ZONE_ALIGNMENT/2)
            return allocate_very_large_object(size);
        size_t index;
        for (index = 0; index < 6; index++) {
            if (lists[index].can_allocate(size)) {
                MarkSweepList* list = &lists[index];
                GVMT_Object result = list->allocate();
                if (result == NULL) { // List full
                    Block *b = Heap::get_block(Space::LARGE, force);
                    if (b == NULL)
                        return NULL;
                    list->add_block(b);
                    result = list->allocate();
                }
                return result;
            }  
        }
        return allocate_big_object(size);
    }
    
    static inline GVMT_Object grey(GVMT_Object obj) {
        Address addr = gc::untag(obj);
        assert(in(addr));
        if (!Zone::marked(addr)) {
            Zone::mark(addr);
            assert(Zone::marked(addr));
            GC::push_mark_stack(obj);
        }
        return obj;
    }
    
    static inline bool is_live(GVMT_Object obj) {
        Address addr = gc::untag(obj);
        return Zone::marked(addr);
    }
    
    static void pre_collection() {
        for (int i = 0; i < 6; i++)
            lists[i].pre_collection();
    }
    
    static void sweep() {
        assert(initialised);
        for (int i = 0; i < 6; i++)
            lists[i].sweep();
        sweep_big_objects();
        sweep_very_large_objects();
    }

    template <class Collection> static void process_old_young() {
        for (int i = 0; i < 6; i++)
            lists[i].process_old_young<Collection>();
        BigObject* obj = big_objects.next;
        while (obj) {
            char* object = &obj->object;
            Zone* z = Zone::containing(object);
            Line* line = Line::containing(object);
            if (z->modified(line)) {
                gc::scan_object<Collection>(object);
                z->clear_modified(line);
            }
            obj = obj->next;
        }
        VeryLargeObject* vl_obj = very_large_objects;
        while (vl_obj) {
            char* object = &vl_obj->object;
            Zone* z = Zone::containing(object);
            Line* line = Line::containing(object);
            if (z->modified(line)) {
                gc::scan_object<Collection>(object);
                z->clear_modified(line);
            }
            vl_obj = vl_obj->next;
        }
    }

};

MarkSweepList LargeObjectSpace::lists[6];
BigObject LargeObjectSpace::big_objects;
VeryLargeObject* LargeObjectSpace::very_large_objects;
bool LargeObjectSpace::initialised = false;


#endif // GVMT_INTERNAL_LARGE_OBJECT_SPACE_H 
