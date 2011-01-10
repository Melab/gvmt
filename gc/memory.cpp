
#include "gvmt/internal/memory.hpp"

void Block::clear_modified_map() {
    Zone* z = Zone::containing(this);
    size_t start = Zone::index_of<Line>(Address(this));
    size_t end = start + Block::size/Line::size;
    for(size_t i = start; i < end; i += 16) {
        int64_t* ptr = (int64_t*)&z->modified_map[i];
        ptr[0] = 0;
        ptr[1] = 0;
    }
}

/** These are posix specific, will need new version for MS Windows */ 

char* OS::get_new_mmap_region(uintptr_t size) {
    // Try to request exact size. 
    // Will usually work as previous requests have been aligned
    char* ptr = (char*)mmap(NULL, size, PROT_READ|PROT_WRITE, 
                            MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (ptr != NULL && Zone::starts_at(ptr)) return ptr;
    munmap(ptr, size);
    // Try again, over allocating to ensure alignment.
    uintptr_t alloc_size = size + Zone::size - getpagesize();
    ptr = (char*)mmap(NULL, alloc_size, PROT_READ|PROT_WRITE, 
                            MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (ptr == NULL) return NULL;
    // Now adjust to be aligned.
    char* start = reinterpret_cast<char*>(Zone::containing(ptr-1)->next());
    // Trim start
    if (start != ptr) {
        assert(ptr < start);
        /* This should not fail */ 
        if (munmap(ptr, start-ptr)) abort();
    }
    // Trim end
    char* end = ptr + alloc_size;
    if (start + size != end) {
        assert(start+size < end);
        /* This should not fail */ 
        if (munmap(start + size, end - (start + size))) abort(); 
    }
    return start;
}

void OS::free_virtual_memory(Zone* zone, size_t size) {
    size_t actual_size = (size + (Zone::size - 1)) & -Zone::size;
    /* This should not fail */ 
    if (munmap(reinterpret_cast<char*>(zone), actual_size) == 0)
        gvmt_virtual_heap_size -= actual_size;
}

Zone* OS::allocate_virtual_memory(size_t size = Zone::size) {
    size_t actual_size = (size + (Zone::size - 1)) & -Zone::size;
    // Attempt to get memory.
    char* ptr = get_new_mmap_region(actual_size);
    if (ptr != NULL) {
        gvmt_virtual_heap_size += actual_size;
    }
    return reinterpret_cast<Zone*>(ptr);
}

#ifdef NDEBUG
void Zone::verify() {}
#else
void Zone::verify() {
    assert (containing(this) == this); 
    Zone* z = 0;
    Address start = z->first()->start();
    size_t start_line_index = Zone::index_of<Line>(start);
    assert(((uint8_t*)(&z->real_blocks)) < &z->modified_map[start_line_index]);
    assert(((uint8_t*)&z->collector_block_data[(Zone::size-1)/Block::size]) < &z->collector_line_data[start_line_index]);
    assert(&z->block_pinned[(Zone::size-1)/Block::size] < &z->pinned[start_line_index]);
    assert(&z->pinned[(Zone::size-1)/Block::size] < z->mark_map);
    assert(((uintptr_t)&z->mark_map[(Zone::size-1)/(8*sizeof(void*))]) < start.bits());
    verify_header();
    verify_layout();
}
#endif

void Zone::verify_heap() {
    Zone *z;
    z = (Zone*)&gvmt_start_heap;
    while (z < Zone::containing((char*)&gvmt_large_object_area_start)) {
        z->verify();
        assert(z->permanent);
        z = z->next();
    }
}

void Zone::verify_header() {
}

void Zone::verify_layout() {
    size_t i;
    for (i = 2; i < real_blocks; i++)
        blocks[i].verify();
}

void Zone::print_blocks() {
    size_t i;
    const char* chars = "pn0LM"+2;
    fprintf(stdout, "%x ", (uintptr_t)this);
    fputc('X', stdout);
    fputc('X', stdout);
    for (i = 2; i < real_blocks; i++) {
        if ((i & 3) == 0)
            fputc(' ', stdout);
        fputc(chars[blocks[i].space()], stdout);
    }
    for (; i < Zone::size/Block::size; i++) {
        if ((i & 3) == 0)
            fputc(' ', stdout);
        fputc('.', stdout);
    }
    fprintf(stdout, "\n");
}

#ifndef NDEBUG
   
bool Block::is_valid() {
    Zone *z = Zone::containing(this);
    if (!Heap::contains(z))
        return false;
    if (this < z->first())
        return false;
    if (this != Block::containing(this))
        return false;
    return true;
}

void Block::verify() {
    assert(is_valid());
    if (space() ==  Space::FREE) {
        if (ring_next == NULL) {
            // Cannot be first block in a group of free blocks.   
            assert((this-1)->space() == Space::FREE);
        } else {
            assert(ring_next->space() == Space::FREE);
            assert(ring_previous->space() == Space::FREE);
        }
    } else {
        Address p = start();
        Line* current_line = NULL;
        while (p < end()) {
            if (p.read_word() == 0) {
                p = Line::containing(p)->end();
            } else {                
                size_t size = align(gvmt_user_length(p.as_object()));
                assert(!Block::crosses(p, size));
                if (Line::containing(p) != current_line) {
                    current_line = Line::containing(p);
                }
                Address end = p.plus_bytes(size);
                Line* l = Line::containing(p)->next();
                while (l->end() < end) {
                    l = l->next();
                }
                p = end;
            }
        }
    }
}

#endif

/** Diagnostic functions */

bool Zone::valid_address(Address addr) {
    return Zone::index_of<Block>(addr) >= 2;
}

bool Heap::contains(Zone* z) {
    for (std::vector<Zone*>::iterator it = zones.begin(); it != zones.end(); it++) {
        if (*it == z)
            return true;
    }
    return false;
}

const char* Space::area_names[] = { "Pinned", "Nursery", "Free", 
                                    "Large-objects", "Mature" };   

const char* Space::area_name(Address a) {
    return area_names[Block::space_of(a)+2];
}

void Zone::print_flags(Address addr) {
    Zone* z = containing(addr);   
    Line * line = Line::containing(addr);
    printf("Permanent: %s\n", z->permanent ? "true" : "false");
    printf("Line start: %x\n", (int)Line::containing(addr));
    printf("Block start: %x\n", (int)Block::containing(addr));
    printf("Area: %s\n", Space::area_name(addr));
    printf("Line modified: %s\n", z->modified(line) ? "true" : "false");
    printf("Collector line data: %d\n", 
           z->collector_line_data[Zone::index_of<Line>(addr)]);
    uint32_t* bd = &z->collector_block_data[Zone::index_of<Block>(addr)];
    uint8_t* bdc = (uint8_t*)bd;
    printf("Collector block data: %d,%d,%d,%d (%d)\n", 
            bdc[0], bdc[1], bdc[2], bdc[3], bd[0]);
    printf("Line pinned: %s\n", 
           z->pinned[Zone::index_of<Line>(addr)] ? "true" : "false");
    printf("Block pinned: %s\n", 
           Block::containing(addr)->is_pinned() ? "true" : "false");
    printf("Marked: %s\n", z->marked(addr) ? "true" : "false");
}

void Heap::print_blocks() {
    for(Heap::iterator it = Heap::begin(); it != Heap::end(); ++it) {
        (*it)->print_blocks();
    }
}

std::vector<Zone*> Heap::zones;
size_t Heap::free_block_count;
std::vector<Block*> Heap::first_free_blocks;
Block* Heap::free_block_rings[ZONE_ALIGNMENT/BLOCK_SIZE];


extern "C" {
 
    void gvmt_memory_debug_flags(void* p) {
        Address a = Address(p);
        Zone::print_flags(a);
        printf("Space: %s\n", Space::area_name(a));
    }
    
}

