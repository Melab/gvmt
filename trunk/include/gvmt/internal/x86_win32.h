#ifdef GVMT_X86_WIN32_H
#error Multiple inclusion of architecture header
#else
#define GVMT_X86_WIN32_H

#define GVMT_CALL __fastcall
               
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

// Fix me - Use Visual C intrinsic?
#warning "No fast compare&swap for your platform, using slower version"
#define HAVE_COMPARE_AND_SWAP 0


#define GVMT_THREAD_LOCAL __declspec( thread )

// Improve this using bswap intrinsic
#define _gvmt_fetch_4(x) \
    ((x[0] << 24) | (x[1] << 16) | (x[2] << 8) | x[3])




#endif // GVMT_X86_WIN32_H

