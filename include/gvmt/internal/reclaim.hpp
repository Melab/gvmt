#ifndef GVMT_INTERNAL_RECLAIM_H 
#define GVMT_INTERNAL_RECLAIM_H 

namespace GC {
    /** Reclaim all white objects */
    template <class Policy> static void reclaim() {
        Policy::sanity();
        for (Heap::iterator it = Heap::begin(); it != Heap::end(); ++it) {
            for (Block* b = (*it)->first(); b != (*it)->end(); b++) {
                int space = b->space();
                if (space == Space::MATURE)
                    Policy::reclaim(b);
            }
        }
        Policy::sanity();
    }

}

#endif // GVMT_INTERNAL_RECLAIM_H 
