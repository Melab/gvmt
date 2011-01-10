/** Immix collector class.
 *  Implements Policy interface
 */
 
#ifndef GVMT_INTERNAL_IMMIX_H 
#define GVMT_INTERNAL_IMMIX_H 

#include <deque>
#include "gvmt/internal/gc.hpp"
#include "gvmt/internal/gc_templates.hpp"
#include "gvmt/internal/memory.hpp"

enum {
    FREE = 1,
    RECYCLE = 2,
    FULL = 3,
    IN_USE = 4,
    EVACUATE = 5
};

enum {
    HEAP = 6,
    PINNED = 7
};

struct BlockData {
    uint8_t use;
    uint8_t free_lines;
    uint8_t pad1;
    uint8_t pad2;
};

#define SENTINEL_VALUE 12345678

class Immix {

    static size_t sentinel1;
    static int available_space_estimate;
    static size_t sentinel2;
    static std::vector<Block*> recycle_blocks;
    static size_t sentinel3;
    static std::vector<Block*> evacuate_blocks;
    static size_t sentinel4;
    static size_t next_block_index;
    static size_t sentinel5;
    static Address free_ptr;
    static size_t sentinel6;
    static Address limit_ptr;
    static size_t sentinel7;
    static Address reserve_ptr;
    static size_t sentinel8;
    
    static inline bool line_marked(Address addr) {
        Zone *z = Zone::containing(addr);
        uintptr_t index = Zone::index_of<Line>(addr);
        return z->collector_line_data[index] != 0;
    }
    
    static void clear_mark_lines(Block* b) {
        Zone *z = Zone::containing(b);
        Address a = b->start();
        Address end = b->end();
        for(; a < end; a = a.plus_bytes(Line::size)) {
            uintptr_t index = Zone::index_of<Line>(a);
            z->collector_line_data[index] = 0; 
        }
    }
    
    static inline BlockData* get_block_data(Address a) {
        Zone *z = Zone::containing(a);   
        uintptr_t index = Zone::index_of<Block>(a);
        uint32_t* bd = &z->collector_block_data[index];
        assert(sizeof(BlockData) <= sizeof(uint32_t));
        return reinterpret_cast<BlockData*>(bd);
    }
    
    static inline void set_block_use(Address a, char use) {
        BlockData* bd = get_block_data(a);   
        bd->use = use;
    }
    
    static inline char get_block_use(Address a) {
        BlockData* bd = get_block_data(a);   
        return bd->use;
    }
    
    static bool no_empty_blocks() {
        for(Heap::iterator it = Heap::begin(); it != Heap::end(); ++it) {
            for (Block* b = (*it)->first(); b != (*it)->first_virtual(); b++) {
                if (b->space() == Space::MATURE) {
                    int lines = 0;
                    for (Line* l = Line::containing(b->start()); l < (Line*)b->next(); l++) {
                        if (line_marked(l->start())) {
                            ++lines;
                        }
                    }
                    if (lines > 0)
                        return true;
                }
            }
        }
        return false;
    }

  
    static Block* get_block() {
        Block* b = Heap::get_block(Space::MATURE, true);
        clear_mark_lines(b);
        set_block_use(b->start(), IN_USE);
        return b;
    }
    
    static void hole_in_new_block() {     
        Block* b;
        if (next_block_index >= recycle_blocks.size()) {
            b = get_block();
            free_ptr = b->start();
            limit_ptr = b->end();
        } else {
            b = recycle_blocks[next_block_index];
            next_block_index++;
            assert(verify_recycle_block(b));
            get_block_data(b->start())->use = IN_USE;
            available_space_estimate -= get_block_data(b->start())->free_lines * Line::size;
            free_ptr = b->start();
            while (line_marked(free_ptr)) {
                free_ptr = free_ptr.plus_bytes(Line::size);
            }
            limit_ptr = free_ptr.plus_bytes(Line::size);
            while (!Block::starts_at(limit_ptr) && !line_marked(limit_ptr)) {
                limit_ptr = limit_ptr.plus_bytes(Line::size);
            }
            assert(Block::containing(free_ptr) == b);
        }  
        assert(free_ptr <= limit_ptr);
        assert(!line_marked(free_ptr));
        assert(Block::starts_at(limit_ptr) || line_marked(limit_ptr));
        assert(get_block_use(free_ptr) != RECYCLE);
    }
    
    static inline void find_new_hole() {
        // This can be inproved with some bit-twiddling.
        // Just lookling for start and length of sequence of zeroes.
        if (!Line::starts_at(free_ptr))
            free_ptr.write_word(0);
        free_ptr = limit_ptr;
        do {
            if (Block::starts_at(free_ptr)) {
                hole_in_new_block();
                return;
            }
            if (!line_marked(free_ptr)) {
                break;
            }
            free_ptr = free_ptr.plus_bytes(Line::size);
        } while (1);
        limit_ptr = free_ptr.plus_bytes(Line::size);
        while (!Block::starts_at(limit_ptr) && !line_marked(limit_ptr)) {
            limit_ptr = limit_ptr.plus_bytes(Line::size);
        }
        assert(free_ptr <= limit_ptr);
        assert(!line_marked(free_ptr));
        assert(Block::starts_at(limit_ptr) || line_marked(limit_ptr));
        assert(get_block_use(free_ptr) != RECYCLE);
    }
    
    static void reclaim(Block* b) {
        unsigned used_lines = 0;
        unsigned index, start = Zone::index_of<Line>(Address(b));
        Zone *z = Zone::containing(b->start());
        int last_line = 0;
        int frag = 0;
        //if (b->is_pinned()) {
        //    int any_pinned = 0;
        //    for(index = 0; index < Block::size/Line::size; ++index) {
        //        int l = z->collector_line_data[start+index];
        //        int p = z->pinned[start+index] & l;
        //        z->pinned[start+index] = p;
        //        any_pinned |= p;
        //        used_lines += l;
        //        frag += (l > last_line);
        //        last_line = l;
        //    }
        //    if (!any_pinned) {
        //        b->set_pinned(false);   
        //    }
        //} else {
            for(index = 0; index < Block::size/Line::size; ++index) {
                int l = z->collector_line_data[start+index];
                used_lines += l;
                frag += (l > last_line);
                last_line = l;
            }
        //}
        unsigned free_lines = Block::size/Line::size - used_lines;
        if (free_lines == 0) {
            set_block_use(b->start(), FULL);
        } else if (used_lines == 0) {
            Heap::free_blocks(b, 1);
            return;
        //} else if (frag >= 4 && !b->is_pinned()) {
        //    // Don't count this block as available_space
        //    set_block_use(b->start(), EVACUATE);
        //    evacuate_blocks.push_back(b);
        } else {
            set_block_use(b->start(), RECYCLE);
            assert(b != Block::containing(free_ptr));
            available_space_estimate += free_lines * Line::size;
            get_block_data(b->start())->free_lines = free_lines;
            assert(verify_recycle_block(b));
            recycle_blocks.push_back(b);
        }
    }
    
    static void markup_block(Block* b) {
        Line* l = reinterpret_cast<Line*>(b);
        while(l < reinterpret_cast<Line*>(b->next())) {
            Address obj = l->start();
            while (obj.read_word()) { 
                Zone::mark(obj);
                obj = obj.plus_bytes(gvmt_user_length(obj.as_object()));
                if (Block::containing(obj) != b) {
                    assert(obj == b->next()->start());
                    return;
                }
            }
            l = Line::containing(obj)->next();
        }
    }
    
    static int correctly_marked(Block* b) {
        Line* l = reinterpret_cast<Line*>(b);
        while(l < reinterpret_cast<Line*>(b->next())) {
            Address obj = l->start();
            while (obj.read_word()) {
                assert(Zone::marked(obj));
                Address end = obj.plus_bytes(gvmt_user_length(obj.as_object()));
                obj = obj.next_word();
                while (obj < end) {
                    assert(!Zone::marked(obj));
                    obj = obj.next_word();
                }
                assert(obj == end);
                if (Block::containing(obj) != b) {
                    assert(obj == b->next()->start());
                    return 1;
                }
            }
            assert(!Zone::marked(obj));
            l = Line::containing(obj)->next(); 
        }
        return 1;
    }
    
    static int verify_recycle_block(Block* b) {
        assert(get_block_use(b->start()) == RECYCLE);
        Address addr = b->start();
        while (addr < b->next()->start()) {
            if (Zone::marked(addr)) {
                size_t len = gvmt_user_length(addr.as_object());
                Address end = addr.plus_bytes(len);
                Address word = addr;
                do {
                    assert(line_marked(word));
                    word = word.next_word();
                } while (word < end);
                addr = end;
            } else {
                addr = addr.next_word();
            }
        }
        Line *l = Line::containing(b);
        Line *end = Line::containing(b->next());
        size_t free = 0;
        for (; l < end; l++) {
            int marked = line_marked(l->start());
            free += (marked == 0);
        }
        assert(get_block_data(b->start())->free_lines == free);
        return 1;
    }
    
public:
     
    static void reclaim() {
        free_ptr = 0;
        limit_ptr = 0;
        // Free evacuated blocks.
        for (std::vector<Block*>::iterator it = evacuate_blocks.begin();
            it != evacuate_blocks.end(); ++it) {
            Block* b = *it;
            assert(!b->is_pinned());
            assert(get_block_data(b->start())->use == EVACUATE);
            Zone::clear_mark_map(b);
            Heap::free_blocks(b, 1);
            assert(b->space() != Space::MATURE);
        }
        evacuate_blocks.clear();
        assert(recycle_blocks.size() == 0);
        for (Heap::iterator it = Heap::begin(); it != Heap::end(); ++it) {
            for (Block* b = (*it)->first(); b != (*it)->first_virtual(); b++) {
                int space = b->space();
                if (space == Space::MATURE)
                    reclaim(b);
            }
        }
        sanity();
        assert(no_empty_blocks());
        assert(safe_state());
    }
    
    static int safe_state() {
        size_t recyclables = 0;
        for(Heap:: iterator it = Heap::begin(); it != Heap::end(); ++it) {
            for (Block* b = (*it)->first(); b != (*it)->first_virtual(); b++) {
                if (b->space() == Space::MATURE) {
                    assert(b->is_valid());
                    int use = get_block_use(b->start());
                    if (use == RECYCLE) {
                        recyclables++;
                        verify_recycle_block(b);
                    }
                }
            }
        }
        assert(recycle_blocks.size()-next_block_index == recyclables);
        return 1;
    }

#ifdef NDEBUG
    static inline void sanity() { }
#else
    static void sanity() {
        assert(sentinel1 == SENTINEL_VALUE);
        assert(sentinel2 == SENTINEL_VALUE);
        assert(sentinel3 == SENTINEL_VALUE);
        assert(sentinel4 == SENTINEL_VALUE);
        assert(sentinel5 == SENTINEL_VALUE);
        assert(sentinel6 == SENTINEL_VALUE);
        assert(sentinel7 == SENTINEL_VALUE);
        assert(sentinel8 == SENTINEL_VALUE);
        size_t recycles = 0;
        assert(available_space_estimate >= 0);  
        std::vector<Zone*>::iterator it, end;
        for(Heap:: iterator it = Heap::begin(); it != Heap::end(); ++it) {
            for (Block* b = (*it)->first(); b != (*it)->first_virtual(); b++) {
                if (b->space() == Space::MATURE) {
                    assert(b->is_valid());
                    int use = get_block_use(b->start());
                    assert(use >= RECYCLE && use <= EVACUATE);
                    if (use == RECYCLE) {
                        recycles++;
                    }
                }
            }
        }
        assert(recycles == recycle_blocks.size()-next_block_index);
        for (size_t i = next_block_index; i < recycle_blocks.size(); ++i) {
            Block* b = recycle_blocks[i];
            assert(verify_recycle_block(b));   
        }
    }
#endif
     
    static void data_block(Block* b) {
        assert(b->is_valid());
        assert(!b->is_pinned());
        markup_block(b);
        b->set_space(Space::MATURE);
        set_block_use(b->start(), FULL);
        assert(correctly_marked(b));
    }

     /** Called once during VM initialisation */
    static void init(size_t heap_size_hint) {
        assert(free_ptr == 0);
        assert(limit_ptr == 0);
        assert(reserve_ptr == 0);
        assert(next_block_index == 0);
        assert(recycle_blocks.size() == 0);
        available_space_estimate = 0;
        sanity();
    }
    
    /** Prepare for collection - All objects should be white.*/
    static void pre_collection() {
        sanity();
        assert(safe_state());
        recycle_blocks.clear();
        free_ptr = 0;
        limit_ptr = 0;
        reserve_ptr = 0;
        next_block_index = 0;
        assert(recycle_blocks.size() == 0);
        available_space_estimate = 0;
        for (std::vector<Block*>::iterator it = evacuate_blocks.begin();
            it != evacuate_blocks.end(); it++) {
            Block* b = *it;
            assert(!b->is_pinned());
            assert(b->space() == Space::MATURE);
            assert(get_block_data(b->start())->use == EVACUATE);
            b->set_space(Space::NURSERY);
            // Set space to Nursery to speed collection,
            // but Immix retains responsibility for the block and 
            // must free it after the collection.
        }
        for(Heap::iterator it = Heap::begin(); it != Heap::end(); ++it) {
            for (Block* b = (*it)->first(); b != (*it)->first_virtual(); b++) {
                if (b->space() == Space::MATURE) {
                    Zone::clear_mark_map(b);
                    clear_mark_lines(b);
                }
            }
        }   
    }
 
    static inline Address allocate(size_t size) {
        Address allocated;
        assert(free_ptr <= limit_ptr);
        assert(free_ptr == Block::containing(free_ptr)->start() ||
               get_block_use(free_ptr) != RECYCLE);
        if (free_ptr.plus_bytes(size) > limit_ptr) {
            if (size > Line::size) {
                assert(size <= Block::size);
                if (!Block::can_allocate(reserve_ptr, size)) {
                    if (!Block::starts_at(reserve_ptr))
                        reserve_ptr.write_word(0);
                    Block *b = get_block();
                    reserve_ptr = b->start();
                }
                allocated = reserve_ptr;
                reserve_ptr = reserve_ptr.plus_bytes(size);
                assert(allocated != Address());
                return allocated;
            } else {
                find_new_hole();
                assert(get_block_use(free_ptr) != RECYCLE);
            }
        }
        allocated = free_ptr;
        free_ptr = free_ptr.plus_bytes(size);
        assert(free_ptr <= limit_ptr);
        assert(allocated != Address());
        assert(get_block_use(allocated) != RECYCLE);
        return allocated;
    }

    /** Return true if this object is grey or black */
    static inline bool is_live(Address obj) {
        return Zone::marked(obj);
    }
     
    /** Returns the available space in bytes */
    static inline size_t available_space() {
        return available_space_estimate < 0 ? 0 : available_space_estimate;
    }
    
    static inline void scanned(Address obj, Address end) {
        Zone* z = Zone::containing(obj);
        Line* l = Line::containing(obj);
        assert(end == obj.plus_bytes(gvmt_user_length(obj.as_object())));
        assert(Zone::marked(obj));
        do {
            z->collector_line_data[Zone::index_of<Line>(l)] = 1;
            l = l->next();
        } while (l->start() < end);
    }
    
    /** Mark this object as grey, that is live, but not scanned */
    static inline GVMT_Object grey(Address addr) {
        if (Block::containing(addr)->space() == Space::NURSERY) {
            assert(!Block::containing(addr)->is_pinned());
            return Memory::copy<Immix>(addr);
        } else {
            if (!Zone::marked(addr)) {
                Zone::mark(addr);
                GC::push_mark_stack(addr);
            }
            return addr.as_object();
        }
    }
      
    static void promote_pinned_block(Block *b) {
        assert(b->space() == Space::PINNED);
        assert(b->is_pinned());
        b->set_space(Space::MATURE);
        reclaim(b);
    }
    
    static inline void pin(GVMT_Object obj) {
        Address addr = Address(obj);
        Zone* z = Zone::containing(addr);
        Block* b = Block::containing(addr);
        assert(b->is_valid());
        z->pinned[Zone::index_of<Line>(addr)] = 1;
        if (b->is_pinned())
            return;
        b->set_pinned(true);
        if (get_block_data(addr)->use == EVACUATE) {
            // Need to remove block from evacuate vector.
            int size = evacuate_blocks.size();
            for (int i = 0; i < size; i++) {
                if (evacuate_blocks[i] == b) {
                    evacuate_blocks[i] = evacuate_blocks.back();
                    evacuate_blocks.pop_back();
                    break;
                }
            }
            set_block_use(addr, IN_USE);
        }
    }
    
    static size_t total_residency() {
        size_t residency = 0;
        for(Heap::iterator it = Heap::begin(); it != Heap::end(); ++it) {
            for (Block* b = (*it)->first(); b != (*it)->first_virtual(); b++) {
                if (b->space() == Space::MATURE) {
                    for (Line* l = Line::containing(b->start()); l < (Line*)b->next(); l++) {
                        if (line_marked(l->start()))
                            residency += Line::size;
                    }
                }
            }
        }
        return residency;
    }
    
    static void view_space() {
        for(Heap::iterator it = Heap::begin(); it != Heap::end(); ++it) {
            for (Block* b = (*it)->first(); b != (*it)->first_virtual(); b++) {
                if (b->space() == Space::MATURE) {
                    for (Line* l = Line::containing(b->start()); l < (Line*)b->next(); l++) {
                        if (line_marked(l->start())) {
                            if (Zone::containing(l)->pinned[Zone::index_of<Line>(l)]) {
                                fputc('P', stdout);
                            } else {
                                fputc('L', stdout);
                            }
                        } else {
                            fputc('.', stdout);
                        }
                    }
                    fprintf(stdout, "\n");
                }
            }
        }
    }

};

size_t Immix::sentinel1 = SENTINEL_VALUE;
int Immix::available_space_estimate;
size_t Immix::sentinel2 = SENTINEL_VALUE;
std::vector<Block*> Immix::recycle_blocks;
size_t Immix::sentinel3 = SENTINEL_VALUE;
std::vector<Block*> Immix::evacuate_blocks;
size_t Immix::sentinel4 = SENTINEL_VALUE;
size_t Immix::next_block_index;
size_t Immix::sentinel5 = SENTINEL_VALUE;
Address Immix::free_ptr;
size_t Immix::sentinel6 = SENTINEL_VALUE;
Address Immix::limit_ptr;
size_t Immix::sentinel7 = SENTINEL_VALUE;
Address Immix::reserve_ptr;
size_t Immix::sentinel8 = SENTINEL_VALUE;


#endif // GVMT_INTERNAL_IMMIX_H 

