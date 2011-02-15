/** Very large objects require at least one zone each and get their memory
 * directly from the OS.
 *
 */
 
struct HugeObject {
    union {
        struct {
            union {
                // Ensure block aligned
                char pad1[Block::size];
                // Ensure space for card-mark table
                uint8_t pad2[Zone::size/Line::size];  
            };
            HugeObject* next;
            size_t size;
        };
        char pad[Block::size*2];
    };
    char object;
};

class HugeObjectSpace {
 
    static HugeObject* young_objects;
    static HugeObject* objects;
    static size_t allocated_space;
    
    static void append_young() {
        if (objects == NULL) {
            objects = young_objects;
        } else {
            HugeObject* obj = objects; 
            while (obj->next) {
                obj = obj->next;
            }
            obj->next = young_objects;
        }
        young_objects = NULL;
    }
    
public:
    
    static void pre_collection() {
        append_young();
        HugeObject* obj = objects;
        while (obj) {
            Zone::unmark(&obj->object);
            assert(!Zone::marked(&obj->object));
            obj = obj->next;
        }
        allocated_space = 0;
    }
    
    static size_t allocated_space_since_collection() {
        return allocated_space;
    }
    
    static GVMT_Object allocate(size_t size) {
        assert(size >= Zone::size/2);
        assert(size < (1 << 29));
        allocated_space += size;
        size += Block::size*2;
        HugeObject* obj = (HugeObject*)OS::allocate_virtual_memory(size);
        gvmt_real_heap_size += size;
        obj->size = size;
        obj->next = objects;
        objects = obj;
        char* start = &obj->object;
        Block::containing(start)->set_space(Space::LARGE);
        return reinterpret_cast<GVMT_Object>(start);
    }
    
    static void sweep() {
        HugeObject* prev = NULL;
        HugeObject* obj = objects;
        HugeObject* next;
        while (obj) {
            next = obj->next;
            if (Zone::marked(&obj->object)) {
                Zone::unmark(&obj->object);
                assert(!Zone::marked(&obj->object));
                prev = obj;
            } else {
                if (prev == NULL) 
                    objects = next;
                else
                    prev->next = next;
                gvmt_real_heap_size -= obj->size;
                OS::free_virtual_memory(reinterpret_cast<Zone*>(obj), obj->size);
            }
            obj = next;
        }
    }
    
    template <class Collection> static void process_old_young() {
        HugeObject* obj = young_objects;
        while (obj) {
            char* object = &obj->object;
            Zone* z = Zone::containing(object);
            Line* line = Line::containing(object);
            gc::scan_object<Collection>(object);
            z->clear_modified(line);
            obj = obj->next;
        }
        obj = objects;
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
        append_young();
    }
    
    static void print_blocks() {
        HugeObject* obj = objects;
        while (obj) {
            size_t size = obj->size;
            Zone* z = Zone::containing(obj);
            fprintf(stdout, "%x ", (uintptr_t)z);
            fputc('X', stdout);
            fputc('X', stdout);
            Block *b = z->first();
            Block* end = Block::containing(((char*)obj)+size-1);
            while (b <= end) {
                if (Zone::index_of<Block>(b) == 0) {
                    fprintf(stdout, "\n");
                    fprintf(stdout, "%x ", (uintptr_t)b);
                } else if ((Zone::index_of<Block>(b) & 3) == 0)
                    fputc(' ', stdout);
                fputc('H', stdout);
                b = b->next();
            }
            while (b < (Block*)Zone::containing(end)->next()) {
                if (Zone::index_of<Block>(b) == 0) {
                    fprintf(stdout, "\n");
                    fprintf(stdout, "%x ", (uintptr_t)b);
                } else if ((Zone::index_of<Block>(b) & 3) == 0)
                    fputc(' ', stdout);
                fputc('.', stdout);
                b = b->next();
            }
            assert(Zone::index_of<Block>(b) == 0);
            fprintf(stdout, "\n");
            obj = obj->next;
        }
    }
};

HugeObject* HugeObjectSpace::young_objects = NULL;
HugeObject* HugeObjectSpace::objects = NULL;
size_t HugeObjectSpace::allocated_space = 0;

