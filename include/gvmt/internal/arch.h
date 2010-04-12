#ifndef GVMT_ARCH_H
#define GVMT_ARCH_H

/** Architecture (and OS) specific stuff */

#if defined(__GNUC__) && defined(__i386)
#include "x86_posix.h"
#elif defined(WIN32)
#include "x86_win32.h"
# error Unsupported Architecture
#endif

#define _gvmt_fetch_2(x) ((x[0] << 8) | x[1])

// union for double register return

typedef union _ {
    int64_t ret;
    struct {
        void* sp;  
        void* ex;
    } regs;
} gvmt_long_jump_value;

typedef int64_t gvmt_double_return_t;

#endif // GVMT_ARCH_H

