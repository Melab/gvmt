#ifdef GVMT_X86_POSIX_H
#error Multiple inclusion of architectur header
#else
#define GVMT_X86_POSIX_H

#define GVMT_CALL __attribute__((fastcall))

struct gvmt_registers {
    void *eip; 
    void *ebp; 
    void *ebx; 
    void *edi; 
    void *esi; 
    void *esp; 
    void *pad; // ? 
};

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

#define GVMT_THREAD_LOCAL __thread

__attribute__((unused)) static uint32_t gvmt_bswap(uint32_t val)
{
    __asm__("bswap %0" : "+r" (val));
    return val;
}

#define _gvmt_fetch_4(x) gvmt_bswap(*((uint32_t*)x))

#endif // GVMT_X86_POSIX_H

