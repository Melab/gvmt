#ifndef GC_GENERATIONAL_H
#define GC_GENERATIONAL_H

#include <inttypes.h>
    
#define LOG_CARD_SIZE 7
#define CARD_SIZE (1 << LOG_CARD_SIZE)
    
typedef uintptr_t Card;

namespace generational {

    extern char* heap;
    extern char* nursery;
    extern char* nursery_top;
    extern size_t nursery_size;
    
    inline Card card_id(Address p) {
        return p.bits() >> LOG_CARD_SIZE;
    }
        
    void init(size_t heap_size);
    
    static inline bool in_nursery(GVMT_Object p) {
        return (reinterpret_cast<int>(p) & 1) == 0 && 
        p >= (GVMT_Object)nursery && p < (GVMT_Object)nursery_top; 
    }
    
    inline Address first_object_on_card(Card card_id) {
        char start = gvmt_object_start_table_offset[card_id];
        char* card_address = reinterpret_cast<char*>(card_id << 7);
        return Address(card_address + start);
    }
    
    inline void set_first_object_on_card(Card card_id, Address o) {
        int start = o.bits() & 127;
        gvmt_object_start_table_offset[card_id] = start;
        assert(first_object_on_card(card_id) == o);
    }
        
    inline bool in_card(Address addr, Card card_id) {
        unsigned int addr_card = addr.bits() >> 7;
        return addr_card == card_id;
    }
    
    void clear_cards(char* begin, char*end);
    
    inline bool is_marked(Card card_id) {
        return gvmt_xgen_pointer_table_offset[card_id] != 0;
    }
    
    template <class Collection> inline 
    void process_old_young(char* start, char* end) {
        int shape_buffer[16];
        Card card, start_card, end_card;
        start_card = card_id(start);
        end_card = card_id(end-1);
        for (card = start_card; card < end_card; card++) {
            if (is_marked(card)) {
                Address p = first_object_on_card(card);
                do {
                    object::iterator it = object::begin(p, shape_buffer);
                    for (; !it.at_end(); ++it) {
                        if (Collection::wants(*it))
                            *it = Collection::apply(*it);
                    }
                    p = it.end_address();
                } while (in_card(p, card));
            }
        }
        // Check last card, but do not go past end of object.
        if (card == end_card && is_marked(end_card)) {
            Address p = first_object_on_card(end_card);
            while (p < Address(end)) {
                object::iterator it = object::begin(p, shape_buffer);
                for (; !it.at_end(); ++it) {
                    if (Collection::wants(*it))
                        *it = Collection::apply(*it);
                }
                p = it.end_address();
            } 
        }
    }
    
};

#endif // GC_GENERATIONAL_H
