#ifndef GVMT_GC_H
#define GVMT_GC_H

#include "gvmt/internal/core.h"
#include <inttypes.h>
#include <assert.h>
#include "stdio.h"
#include <deque>
#include <vector>

#define FORWARDING_BIT 1

inline size_t align(uintptr_t size) {
    return ((size+(sizeof(void*)-1))&(-(sizeof(void*))));
}

template<class T> inline T* align(T* p) {
   return reinterpret_cast<T*>(align(reinterpret_cast<size_t>(p)));
}

template<class T> inline bool is_aligned(T p) {
    return p == align(p);
}

class Address {
    char* p;
public:
    
    inline bool is_aligned() {
        return ::is_aligned(p); 
    }
    
    inline Address() {
        this->p = 0;   
    }
    
    inline Address(char* p) {
        this->p = p;   
        assert((bits() & (sizeof(void*)-1)) == 0); 
    }
    
    template <class T> explicit Address(T* p) {
        this->p = reinterpret_cast<char*>(p);  
    }
    
    inline Address next_byte() {
        return Address(p+1);   
    }
    
    inline Address plus_bytes(intptr_t i) {
           return Address(p+i);
    }
    
    inline Address next_word() {
        return Address(p+sizeof(void*));   
    }
    
    inline uintptr_t bits() {
        return reinterpret_cast<uintptr_t>(p);   
    }
    
    static Address from_bits(uintptr_t u) {
        return Address(reinterpret_cast<char*>(u));  
    }
    
    inline bool operator == (Address other) {
        return p == other.p;
    }
    
    inline bool operator != (Address other) {
        return p != other.p;
    }
    
    inline bool operator < (Address other) {
        return p < other.p;
    }
    
    inline bool operator > (Address other) {
        return p > other.p;
    }
    
    inline bool operator <= (Address other) {
        return p <= other.p;
    }
    
    inline bool operator >= (Address other) {
        return p >= other.p;
    }
    
    inline uintptr_t read_word() {
        assert(is_aligned());
        return reinterpret_cast<uintptr_t*>(p)[0];
    }
    
    inline void write_word(uintptr_t w) {
        assert(is_aligned());
        reinterpret_cast<uintptr_t*>(p)[0] = w;
    }
    
    inline GVMT_Object read_reference() {
        assert(is_aligned());
        return *reinterpret_cast<GVMT_Object*>(p);
    }
    
    inline void write_reference(GVMT_Object obj) {
        assert(is_aligned());
        *reinterpret_cast<GVMT_Object*>(p) = obj;
    }
    
    inline GVMT_Object as_object() {
        assert(is_aligned());
        return  reinterpret_cast<GVMT_Object>(p);     
    }
    
};  

namespace gc {
#ifdef GVMT_TAGGING
    
    inline bool is_address(GVMT_Object p) { 
        return (p != NULL) && 
        ((reinterpret_cast<intptr_t>(p) & 3) == 0); 
    }
    
    inline bool is_tagged(GVMT_Object p) { 
        return (reinterpret_cast<intptr_t>(p) & 3) != 0; 
    }
    
#else
    
    inline bool is_address(GVMT_Object p) { 
        return p != NULL;
    }
    
    inline bool is_tagged(GVMT_Object p) { 
        return false;
    }

#endif
};

namespace object {
        
    static int zero = 0;
    
    class iterator {
        int*shape;
        int index;
        GVMT_Object* item;
        
    public:
        
        iterator() {
           shape = &zero; 
        }
        
        iterator(Address object, int* shape_buffer) {
            index = 0;
            item = reinterpret_cast<GVMT_Object*>(object.bits());
            shape = gvmt_shape(object.as_object(), shape_buffer);
            while (*shape < 0) {
                item -= *shape;
                ++shape;
            }            
        }
        
        inline iterator& operator++ () {
            ++index;
            ++item;
            if (index < *shape) {
                return *this;
            }
            ++shape;
            while (*shape < 0) {
                item -= *shape;
                ++shape;
            }
            index = 0;
            return *this;
        }
        
        inline bool at_end() {
            return *shape == 0;
        }
        
        /** Moves iterator to the end of the object and 
         * Returns a reference to the memory immediately after 
         * the object. 
         */
        inline Address end_address() {
            while (!at_end())
                ++*(this);
            return Address(item);
        }
        
        inline GVMT_Object& operator*() {
            return *item;
        }
        
        inline bool operator < (GVMT_Object* other) {
            return item < other;
        }
        
    };
    
    inline iterator begin(Address object, int* shape_buffer) {
        return iterator(object, shape_buffer);
    }

};
/** The first root */
extern GVMT_Object gvmt_start_roots;
/** One after the last root */
extern GVMT_Object gvmt_end_roots;
/** First byte in the initial heap */
extern char gvmt_start_heap;
/** First byte after the end of the initial heap */
extern char gvmt_end_heap;

/* If you change this make sure to change 
 * matching constant in gvmt-as tool */
#define ROOT_LIST_BLOCK_SIZE 60

namespace Root {

    struct Block {
        GVMT_Object* start;
        GVMT_Object* end;
        Block *next;
    };
    
    Block* allocateBlock();

    struct List {
        Block* first; 
        Block* last;
        GVMT_Object* end_reference;
        
        void intialise() {
           first = last = allocateBlock();
           end_reference = first->start;     
        }
       
        GVMT_Object* addRoot(GVMT_Object r) {
            if (end_reference == last->end) {
                Block* temp = last->next;
                if (temp == NULL)
                    temp = allocateBlock();
                end_reference = temp->start;
                last->next = temp;
                last = temp;
            }
            GVMT_Object *result = end_reference;
            *result = r;
            ++end_reference;
            return result;
        }     
    
        class iterator {  
            GVMT_Object* root;
            Block* block;   
            
        public:
             
            iterator(Block* b, GVMT_Object* root) {
                this->root = root;
                this->block = b;
            }
            
            inline iterator operator++ () {
                ++root;
                if (root == block->end) {
                    if (block->next) {
                        block = block->next;
                        root = block->start;
                    }
                }
                return *this;
            }
            
            inline GVMT_Object& operator*() {
                return *root;
            }
        
            inline bool operator==(iterator& other) {
                return root == other.root;
            }
            
            inline bool operator!=(iterator& other) {
                return root != other.root;
            }
            
        };    
            
        iterator begin() {
            return iterator(first, first->start);
        }

        iterator end() {
            return iterator(last, end_reference);
        }
        
        inline void pop_back() {
            if (end_reference == last->start) {
                // Final block is empty.
                // Update last and end_reference.
                Block* b = first;
                assert(b != last);
                while (b->next != last)
                    b = b->next;
                last = b;               
                end_reference = last->end;
            }
            --end_reference;
        }
                            
        inline void free(iterator& it) {
            // Copy end back into it.   
            *it = *end_reference;
            pop_back();
        }
        
        inline void free(GVMT_Object* ref) {
            *ref = *end_reference;
            pop_back();
        }
        
    }; 
    
    extern Block GC_ROOTS_BLOCK;
  
    extern List GC_ROOTS;
    
    extern GVMT_THREAD_LOCAL GVMT_Object* TLS_VECTOR;
    
};

class FrameStack {
    
    struct gvmt_frame** top_frame_pointer_addr;
    
public:
      
    FrameStack(struct gvmt_frame** top_frame_pointer_addr) {
        this->top_frame_pointer_addr = top_frame_pointer_addr;
    }
 
    class iterator {
        
        struct gvmt_frame* frame;
        uintptr_t index;
        
    public:
        
        iterator(struct gvmt_frame* frame) {
            this->frame = frame;
            assert(frame == NULL || frame->count > 0);
            index = 0;
        }
        
        inline bool operator==(iterator& other) {
            return frame == other.frame && index == other.index;
        }
        
        inline bool operator!=(iterator& other) {
            return frame != other.frame || index != other.index;
        }
        
        inline GVMT_Object& operator*() {
            GVMT_Object* p = &frame->refs[index];
            return *p;
        }
        
        inline iterator& operator++ () {
            index++;
            if (index == frame->count) {
                frame = frame->previous;
                index = 0;
            }
            return *this;
         }
         
    };
        
    
    inline iterator begin() { 
        return iterator(*top_frame_pointer_addr);
    } 
    
    inline iterator end() { 
        return iterator(NULL);
    }
    
    
};

class Stack {
    
    GVMT_Object* base;
    GVMT_Object** pointer_addr;
        
    public:
    
    Stack(GVMT_StackItem* stack_base,
          GVMT_StackItem** stack_pointer_addr) {
        this->base = (GVMT_Object*)stack_base;
        this->pointer_addr = (GVMT_Object**)stack_pointer_addr;
    }
    
    class iterator {
        GVMT_Object* item;
        
    public:
        
        iterator(GVMT_Object* item) {
            this->item = item;
        }
        
        inline iterator& operator++ () {
            ++item;
            return *this;
        }
        
        inline GVMT_Object& operator*() {
            return *item;
        }
        
        inline bool operator==(iterator& other) {
            return item == other.item;
        }
        
        inline bool operator!=(iterator& other) {
            return item != other.item;
        }
        
    };
     
    inline iterator begin() { 
        return iterator(*pointer_addr);
    } 
    
    inline iterator end() { 
        return iterator(base);
    }

}; 

extern "C" uint32_t size_from_shape(int* shape);

namespace GC {
    
    extern std::vector<Address> mark_stack;
    extern std::deque<GVMT_Object> finalization_queue;
    extern std::vector<Stack> stacks;
    extern std::vector<FrameStack> frames;
    extern std::deque<GVMT_Object> finalizables;
    extern Root::List weak_references;
    
    inline void push_mark_stack(Address addr) {
#ifndef NDEBUG
        int shape_buffer[GVMT_MAX_SHAPE_SIZE];
        assert(size_from_shape(gvmt_shape(addr.as_object(), shape_buffer)) == 
               gvmt_length(addr.as_object())); 
#endif
        mark_stack.push_back(addr);
    }

};

void gvmt_do_collection();

extern "C" {
    
    GVMT_CALL GVMT_StackItem* user_finalise_object(GVMT_StackItem* sp, GVMT_Frame fp);    
    void user_gc_out_of_memory(void);

    extern char* gvmt_xgen_pointer_table_offset;
    extern char* gvmt_object_start_table_offset;
}

inline int64_t high_res_time(void) {
    struct timespec ts; 
    clock_gettime(CLOCK_REALTIME, &ts);
    int64_t billion = 1000000000;
    return billion * ts.tv_sec + ts.tv_nsec;
}

/** Allocator interface */
class Allocator {

    /** Allocate an object. 
     * size must be mulitple of the wordsize.
     * If cannot allocate returns NULL */
     static inline void* allocate(size_t size);

};

class Collector {

    static inline void collect();
    
    /** Called by VM::trace */
    static inline void root(GVMT_Object object);
    
    /** Called by VM::trace */
    static inline void end_trace(GVMT_Object object);
    
    /** Called by VM to handle weak references and finalisers */
    static inline bool is_live(GVMT_Object object);

};

class Policy : public Collector {

    static inline void pin(GVMT_Object object);

};

/** This class is the interface to the VM for the GC.
 * Ie. this class is implemented *for* the GC, not by the GC.
 */

template <class Collector> class VM {
        
    template <class Policy> inline void process_roots(Policy policy) {
        for (Root::List::iterator it = Root::GC_ROOTS.begin(), 
                                  end = Root::GC_ROOTS.end(); it != end; ++it) {
            if (policy.applies(*it))
                *it = policy.apply(*it);
        }
        for (std::vector<FrameStack>::iterator it = GC::frames.begin(),
                          end = GC::frames.end(); it != end; ++it) {
            for (FrameStack::iterator it2 = it->begin(), 
                                  end2 = it->end(); it2 != end2; ++it2) {
                if (policy.applies(*it2))
                    *it2 = policy.apply(*it2);
            }
        } 
        for (std::vector<Stack>::iterator it = GC::stacks.begin(),
                          end = GC::stacks.end(); it != end; ++it) {
            for (Stack::iterator it2 = it->begin(),
                                 end2 = it->end(); it2 != end2; ++it2) {
                if (policy.applies(*it2))
                    *it2 = policy.apply(*it2);
            }
        }
        for (std::deque<GVMT_Object>::iterator it = GC::finalization_queue.begin(), 
                        end = GC::finalization_queue.end(); it != end; ++it) {
            if (policy.applies(*it))
                *it = policy.apply(*it);   
        }
    }
    
    
    template <class Policy> inline void process_finalisers(Policy policy) {
        int n = GC::finalizables.size();
        for(int i = 0; i < n; i++) {
            GVMT_Object obj = GC::finalizables.back();
            GC::finalizables.pop_back();
            if (policy.applies(obj)) {
                obj = policy.apply(obj);
                if (policy.is_live(obj)) {
                    GC::finalizables.push_front(obj);
                } else {
                    // obj is not live, so it needs to be finalized
                    // Keep live, add to finalization Q, then remove from finalizables.
                    GC::finalization_queue.push_front(obj);
                }
            } else {
                GC::finalizables.push_front(obj);  
            }
        }
    }
    
    template <class Policy> inline void process_weak_refs(Policy policy) {
        for (Root::List::iterator it = GC::weak_references.begin(), 
                           end = GC::weak_references.end(); it != end; ++it) {
            // If *it is not live then zero it.
            if (policy.applies(*it)) {  
                if (policy.is_live(*it)) {
                    *it = policy.apply(*it);
                } else {
                    *it = 0;
                }
            }
        }
    }
  
};

#endif

