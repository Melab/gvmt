
#include "gvmt/internal/gc.hpp"

Root::Block Root::GC_ROOTS_BLOCK = {
    &gvmt_start_roots,
    &gvmt_end_roots,
    0
};

Root::List Root::GC_ROOTS = {
    &GC_ROOTS_BLOCK,
    &GC_ROOTS_BLOCK,
    &gvmt_end_roots
};

Root::Block* Root::allocateBlock() {
    Root::Block* b = new Root::Block();
    b->start = new GVMT_Object[ROOT_LIST_BLOCK_SIZE];
    b->end = b->start + ROOT_LIST_BLOCK_SIZE;
    b->next = 0;
    return b;
}

namespace GC {
    
    std::vector<Address> mark_stack;
    std::deque<GVMT_Object> finalization_queue;
    std::vector<Stack> stacks;
    std::vector<FrameStack> frames;
    Root::List finalizers;
    Root::List weak_references;
       
}

