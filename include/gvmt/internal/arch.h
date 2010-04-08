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

#endif // GVMT_ARCH_H

