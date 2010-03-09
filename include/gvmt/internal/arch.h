#ifndef GVMT_ARCH_H
#define GVMT_ARCH_H

/** Architecture (and OS) specific stuff */


// GVMT_CALL should be defined so that at least the first two parameters
// are passed in registers.
#ifdef __GNUC__
#  ifdef  __i386
#    define GVMT_CALL __attribute__((fastcall))
#  else
#    define GVMT_CALL 
#  endif
#else
#  error "Please define GVMT_CALL for your platform. Thanks."
#endif

#ifdef  __i386
//x86 version 
struct gvmt_registers {
    void *eip; 
    void *ebp; 
    void *ebx; 
    void *edi; 
    void *esi; 
    void *esp; 
    void *pad; // ? 
};

#else
#  error "Need to define struct gvmt_registers"
#endif
// Prototype for compare and swap
//extern bool COMPARE_AND_SWAP(int* addr, int old_val, int new_val);

#if defined __GNUC__ && __GNUC__ > 3 && __GNUC_MINOR__ > 0
#  define COMPARE_AND_SWAP(addr, old_val, new_val) \
__sync_bool_compare_and_swap(addr, old_val, new_val)
#  define COMPARE_AND_SWAP_BYTE(addr, old_val, new_val) \
__sync_bool_compare_and_swap(addr, old_val, new_val)
#  define HAVE_COMPARE_AND_SWAP 1
#else
#  warning "No fast compare&swap for your platform, using slower version"
#  define HAVE_COMPARE_AND_SWAP 0
#endif

#ifdef __GNUC__
#  define GVMT_THREAD_LOCAL __thread
#else
#  ifdef WIN
#    define GVMT_THREAD_LOCAL __declspec( thread )
#  else
#    error "Please define GVMT_THREAD_LOCAL for your platform. Thanks."
#  endif
#endif

#endif // GVMT_ARCH_H

