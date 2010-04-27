
#include "gvmt/internal/memory.hpp"

void Block::clear_modified_map() {
    Zone* z = Zone::containing(this);
    size_t start = Zone::index_of<Line>(this);
    size_t end = start + Block::size/Line::size;
    for(size_t i = start; i < end; i += 8) {
        int32_t* ptr = (int32_t*)&z->modified_map[i];
        ptr[0] = 0;
        ptr[1] = 0;
    }
}

void Block::clear_mark_map() {
    int32_t* start = (int32_t*)Zone::mark_byte((char*)this);
    size_t i;
    for (i = 0; i < Block::size/Line::size; i+= 2) {
        start[i] = 0;
        start[i+1] = 0;
    }
}

char* ReservedMemoryHandle::get_new_mmap_region(uintptr_t size) {
    // Try to request exact size. Will generally work as previous requests have been aligned
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

ReservedMemoryHandle ReservedMemoryHandle::allocate(size_t size = Zone::size) {
    size_t actual_size = Zone::size;
    ReservedMemoryHandle result;
    union {
        uintptr_t data;
        void* ptr;
    };
    while (actual_size < size) {
        actual_size += Zone::size;
    }
    // Attempt to get memory.
    ptr = get_new_mmap_region(actual_size);
    if (ptr == NULL) {
        result.data = 0; 
    } else {
        assert((data & (Zone::size - 1)) == 0);
        assert((actual_size >> Zone::log_size) < Zone::size);
        result.data = data | (actual_size >> Zone::log_size);  
    }
    return result;
}

Zone* ReservedMemoryHandle::activate() {
    union {
        Zone* z;
        uintptr_t bits;
    };
    bits = data & (-Zone::size);
    size_t size = (data & (Zone::size-1));
    assert((bits | size) == data);
    gvmt_heap_size += Zone::size * size;
    return z;
}

void Zone::verify() {
    assert (containing(this) == this);   
    verify_header();
    verify_layout();
}

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
    for(i = 2; i < Zone::size/Block::size; i++)
        blocks[i].verify();
}

#ifndef NDEBUG

void Block::verify() {
    Address p = start();
    Line* current_line = NULL;
    while (p < end()) {
        if (p.read_word() == 0) {
            p = Line::containing(p)->end();
        } else {                
            size_t size = align(gvmt_length(p.as_object()));
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

#endif

bool Zone::valid_address(Address addr) {
    return Zone::index_of<Block>(addr) >= 2;
}

bool is_mark(char* mem) {
    return Zone::marked(mem);   
}

const char* Space::area_names[] = { "Pinned", "Nursery", "Mature", "Large-objects" };   

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
    printf("Collector line data: %d\n", z->collector_line_data[Zone::index_of<Line>(addr)]);
    printf("Collector block data: %d\n", z->collector_block_data[Zone::index_of<Block>(addr)]);
    printf("Line pinned: %s\n", z->pinned[Zone::index_of<Line>(addr)] ? "true" : "false");
    printf("Marked: %s\n", z->marked(addr) ? "true" : "false");
}

std::vector<Zone*> Heap::zones;
std::vector<uint32_t> Heap::free_bits;
int Heap::first_free;
int Heap::virtual_zones;
size_t Heap::free_block_count;
size_t Heap::single_block_cursor;
size_t Heap::multi_block_cursor;


