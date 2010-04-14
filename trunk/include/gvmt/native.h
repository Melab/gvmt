#ifndef GVMT_NATIVE_H
#define GVMT_NATIVE_H

#include <pthread.h>
#include "gvmt/internal/shared.h"

#include "gvmt/internal/call.h"

#ifdef __cplusplus
extern "C" {
#endif 

#ifdef GVMT_LINKAGE
#error Native header - Use native (not GVMT) compiler.
#endif

typedef int (*gvmt_func_ptr)(void);

/** Start the abstract machine executing func in current thread.
    This procedure should be called once only and from the main thread.*/
int gvmt_start_machine(uintptr_t stack_space, gvmt_func_ptr func, int pcount, ...);

/** Starts a new thread, running func and passing pcount arguments */
void gvmt_start_thread(uintptr_t stack_space, gvmt_func_ptr func, int pcount, ...);

int gvmt_enter(uintptr_t stack_space, gvmt_func_ptr func, int pcount, ...);

int gvmt_reenter(gvmt_func_ptr func, int pcount, ...);

/** Sets the (thread-local) tracing state to s. */
void gvmt_set_tracing(int s);

/** Returns the (thread-local) tracing state. */
int gvmt_get_tracing(void);

/** Allows exceptions to be raised from native code*/
void gvmt_raise_native(GVMT_Object ex);
void gvmt_transfer_native(GVMT_Object ex);

#ifdef __cplusplus
}
#endif 

#endif 

