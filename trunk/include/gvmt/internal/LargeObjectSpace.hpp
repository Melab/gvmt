#ifndef GVMT_INTERNAL_LARGE_OBJECT_SPACE_H 
#define GVMT_INTERNAL_LARGE_OBJECT_SPACE_H 

#include "gvmt/internal/memory.hpp"
#include "gvmt/internal/MarkSweep.hpp"
#include "gvmt/internal/HugeObjectSpace.hpp"

/** The Large Object Space handles objects from 12k to 256k.
 * (The HugeObject space handles objects > 256k)
 * Large objects are allocated whole block(s), which are requested directly from
 * the Heap object.
 */

struct BigObject {
    BigObject* next;
    char object;
};


class LargeObjectSpace: Space {
    
    static BigObject old_objects;
    static BigObject young_objects;
    static bool initialised;
    
    static void promote_young_objects() {
        BigObject *obj = &old_objects;
        while (obj->next) {
            obj = obj->next;
        }
        obj->next = young_objects.next;
        young_objects.next = NULL;
    }
    
    static inline size_t blocks_for_big_object(size_t size) {
        return (size+sizeof(void*)+(Block::size-1))>>Block::log_size; 
    }
    
    static GVMT_Object allocate_big_object(size_t size, bool force) {
        size_t blocks = blocks_for_big_object(size);
        BigObject* ptr = (BigObject*)Heap::get_blocks(blocks, Space::LARGE, force);
        if (ptr == NULL)
            return NULL;
        ptr->next = young_objects.next;
        young_objects.next = ptr;
        char* c = &ptr->object;
        return reinterpret_cast<GVMT_Object>(c);
    }
    
    static void sweep_old_objects() {
        assert(young_objects.next == NULL);
        BigObject* prev = &old_objects;
        BigObject* obj = prev->next;
        BigObject* next;
        while (obj) {
            next = obj->next;
            if (Zone::marked(&obj->object)) {
                Zone::unmark(&obj->object);
                assert(!Zone::marked(&obj->object));
                prev = obj;
            } else {
                prev->next = next;
                char* c = &obj->object;
                size_t len = align(gvmt_user_length(reinterpret_cast<GVMT_Object>(c)));
                size_t blocks = blocks_for_big_object(len);
                assert(Block::containing((char*)obj) == (Block*)obj);
                Heap::free_blocks((Block*)obj, blocks);
            }
            obj = next;
        }
    }

public:
           
    static inline bool in(Address a) {
        return Block::space_of(a) == Space::LARGE;
    }
    
    static void verify_heap() {
        // Not much we can do for multi-block objects.
    }
    
    static void init() {
        assert(Block::containing(&gvmt_large_object_area_start) == 
               (Block*)&gvmt_large_object_area_start);
        assert(Block::containing(&gvmt_large_object_area_end) == 
               (Block*)&gvmt_large_object_area_end);
        assert(Zone::containing(&gvmt_end_heap) == (Zone*)&gvmt_end_heap);
        // TO DO - Handle multi-block objects at start up.
        // Will need an external list of objects.
        assert(&gvmt_large_object_area_start == &gvmt_large_object_area_end);
        Block* b = (Block*)&gvmt_large_object_area_end;
        Block* end = Block::containing(&gvmt_end_heap);
        Heap::init_free_blocks(b, end - b);
//        lists[0].init(2048);
//        lists[1].init(2720);
//        lists[2].init(4096);
//        lists[3].init(5456);
//        lists[4].init(8192);
//        lists[5].init(10912);
//        Block* b = (Block*)&gvmt_large_object_area_start;
//        assert(b == Block::containing((char*)b));
//        // Skip zone headers
//        if (b < Zone::containing(b)->first())
//            b = Zone::containing(b)->first();
//        
//        // TO DO - Improve passing of free blocks to heap,
//        // by passing in blocks not one at a time.
//        Block* end = Block::containing(&gvmt_multiblock_object_start);
//        assert(end == (Block*)&gvmt_multiblock_object_start);
//        while(b < end) {
//            int8_t area = b->space();
//            if (area == 0) {
//                break;
//            } else {
//                assert(area >= 22);
//                assert(area <= 27);
//                lists[area-22].init_block(b);
//            }
//            b = b->next();
//            // Skip zone headers
//            if (Zone::index_of<Block>((char*)b) == 0)
//                b = Zone::containing(b)->first();
//        }
//        Heap::init_free_blocks(b, end - b);
//        b = end;
//        end = Block::containing(&gvmt_end_heap);
//        assert(end == (Block*)&gvmt_end_heap);
//        if (b < end) {
//            assert(b->space() == -128);
//            old_objects.next = reinterpret_cast<BigObject*>(b);
//        }
//        b = (Block*)&gvmt_large_object_area_end;
//        Heap::init_free_blocks(b, end - b);
        initialised = true;
    }
    
    static GVMT_Object allocate(size_t size, bool force) {
        assert(initialised);
        // Space for mark word.
        if (size >= ZONE_ALIGNMENT/2)
            return HugeObjectSpace::allocate(size);
        return allocate_big_object(size, force);
    }
    
    static inline GVMT_Object grey(Address addr) {
        assert(in(addr));
        if (!Zone::marked(addr)) {
            Zone::mark(addr);
            assert(Zone::marked(addr));
            GC::push_mark_stack(addr);
        }
        return addr.as_object();
    }
    
    static inline bool is_live(Address addr) {
        return Zone::marked(addr);
    }
    
    static void pre_collection() {
        promote_young_objects();
        BigObject* obj = old_objects.next;
        while (obj) {
            Zone::unmark(&obj->object);
            assert(!Zone::marked(&obj->object));
            obj = obj->next;
        }
    }
    
    static void sweep() {
        assert(initialised);
        sweep_old_objects();
    }

    template <class Collection> static void process_old_young() {
        BigObject* obj = young_objects.next;
        while (obj) {
            char* object = &obj->object;
            Zone* z = Zone::containing(object);
            Line* line = Line::containing(object);
            gc::scan_object<Collection>(object);
            z->clear_modified(line);
            obj = obj->next;
        }
        obj = old_objects.next;
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
        promote_young_objects();
    }

};

BigObject LargeObjectSpace::young_objects;
BigObject LargeObjectSpace::old_objects;
bool LargeObjectSpace::initialised = false;


#endif // GVMT_INTERNAL_LARGE_OBJECT_SPACE_H 
