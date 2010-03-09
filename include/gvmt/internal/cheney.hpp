#ifndef CHENEY_H
#define CHENEY_H

namespace cheney {
    
    extern char *free, *top_of_space;

    void init(uint32_t size);
    
    void flip(void);
    
}

#endif
