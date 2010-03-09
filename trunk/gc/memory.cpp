
#include "gvmt/internal/memory.hpp"

void Block::clear_modified_map() {
    SuperBlock* sb = SuperBlock::containing(this);
    size_t start = SuperBlock::index_of<Line>(this);
    size_t end = start + Block::size/Line::size;
    for(size_t i = start; i < end; i += 8) {
        int32_t* ptr = (int32_t*)&sb->modified_map[i];
        ptr[0] = 0;
        ptr[1] = 0;
    }
}

void Block::clear_mark_map() {
    int32_t* start = (int32_t*)SuperBlock::mark_byte((char*)this);
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
    if (ptr != NULL && SuperBlock::starts_at(ptr)) return ptr;
    munmap(ptr, size);
    // Try again, over allocating to ensure alignment.
    uintptr_t alloc_size = size + SuperBlock::size - getpagesize();
    ptr = (char*)mmap(NULL, alloc_size, PROT_READ|PROT_WRITE, 
                            MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (ptr == NULL) return NULL;
    // Now adjust to be aligned.
    char* start = reinterpret_cast<char*>(SuperBlock::containing(ptr-1)->next());
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

ReservedMemoryHandle ReservedMemoryHandle::allocate(size_t size = SuperBlock::size) {
    size_t actual_size = SuperBlock::size;
    ReservedMemoryHandle result;
    union {
        uintptr_t data;
        void* ptr;
    };
    while (actual_size < size) {
        actual_size += SuperBlock::size;
    }
    // Attempt to get memory.
    ptr = get_new_mmap_region(actual_size);
    if (ptr == NULL) {
        result.data = 0; 
    } else {
        assert((data & (SuperBlock::size - 1)) == 0);
        assert((actual_size >> SuperBlock::log_size) < SuperBlock::size);
        result.data = data | (actual_size >> SuperBlock::log_size);  
    }
    return result;
}

SuperBlock* ReservedMemoryHandle::activate() {
    union {
        SuperBlock* sb;
        uintptr_t bits;
    };
    bits = data & (-SuperBlock::size);
    size_t size = (data & (SuperBlock::size-1));
    assert((bits | size) == data);
    gvmt_heap_size += SuperBlock::size * size;
    return sb;
}

void SuperBlock::verify() {
    assert (containing(this) == this);   
    verify_header();
    verify_layout();
}

void SuperBlock::verify_heap() {
    SuperBlock *sb;
    sb = (SuperBlock*)&gvmt_start_heap;
    while (sb < SuperBlock::containing((char*)&gvmt_large_object_area_start)) {
        sb->verify();
        assert(sb->permanent);
        sb = sb->next();
    }
}

void SuperBlock::verify_header() {
}

void SuperBlock::verify_layout() {
    size_t i;
    for(i = 2; i < SuperBlock::size/Block::size; i++)
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

bool SuperBlock::valid_address(Address addr) {
    return SuperBlock::index_of<Block>(addr) >= 2;
}

bool is_mark(char* mem) {
    return SuperBlock::marked(mem);   
}

const char* Space::area_names[] = { "Pinned", "Nursery", "Mature", "Large-objects" };   

const char* Space::area_name(Address a) {
    return area_names[Block::space_of(a)+2];
}

void SuperBlock::print_flags(Address addr) {
    SuperBlock* sb = containing(addr);   
    Line * line = Line::containing(addr);
    printf("Permanent: %s\n", sb->permanent ? "true" : "false");
    printf("Line start: %x\n", (int)Line::containing(addr));
    printf("Block start: %x\n", (int)Block::containing(addr));
    printf("Area: %s\n", Space::area_name(addr));
    printf("Line modified: %s\n", sb->modified(line) ? "true" : "false");
    printf("Collector line data: %d\n", sb->collector_line_data[SuperBlock::index_of<Line>(addr)]);
    printf("Collector block data: %d\n", sb->collector_block_data[SuperBlock::index_of<Block>(addr)]);
    printf("Line pinned: %s\n", sb->pinned[SuperBlock::index_of<Line>(addr)] ? "true" : "false");
    printf("Marked: %s\n", sb->marked(addr) ? "true" : "false");
}
