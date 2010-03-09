#include "gvmt/internal/core.h"
#include <malloc.h>
#include <stdlib.h>
#include <assert.h>

GVMT_THREAD_LOCAL GvmtExceptionHandler gvmt_thread_exception_handler;

void gvmt_pop_handler(void) {
    assert(gvmt_thread_exception_handler != gvmt_thread_exception_handler->pop);
    gvmt_thread_exception_handler = gvmt_thread_exception_handler->pop;
}

static GvmtExceptionHandler gvmt_push_new_handler(void) {
    GvmtExceptionHandler new_handler = malloc(sizeof(struct gvmt_exception_handler));
    GvmtExceptionHandler current = gvmt_thread_exception_handler;
    new_handler->pop = current;
    new_handler->push = 0;
    current->push = new_handler;
    gvmt_thread_exception_handler = new_handler;
    return new_handler;
}

GvmtExceptionHandler gvmt_push_handler(void) {
    GvmtExceptionHandler current = gvmt_thread_exception_handler;
    GvmtExceptionHandler handler = current->push;
    if (handler) 
        gvmt_thread_exception_handler = handler;
    else
        handler = gvmt_push_new_handler();
    return handler;
}

void initialise_exception_stack(void) {
    GvmtExceptionHandler base = malloc(sizeof(struct gvmt_exception_handler));
    base->pop = base;
    base->push = 0;
    gvmt_thread_exception_handler = base;
    GVMT_StackItem* sp = gvmt_stack_pointer;
    void *ex = gvmt_setjump(&base->registers);
    if (ex) {
        // Call user defined thread-specific uncaught exception handler.
        gvmt_stack_pointer = sp;
        // Cleanly dispose of this thread?
        // Tell GC this thread is no longer running.
        gvmt_enter_native(gvmt_stack_pointer, 0);
        // Really ought not to return from here.
        assert(0 && "To do...");
        abort();
    } 
}

void gvmt_raise_exception(GVMT_Object ex) {
    gvmt_longjump(&gvmt_thread_exception_handler->registers, ex);
}

void gvmt_raise_native(void* ex_root) {
    if (gvmt_thread_non_native == 0)
        // May have to wait for GC to complete.
        gvmt_exit_native();
    assert(gvmt_thread_non_native == 1);
    GVMT_Object ex = *((GVMT_Object*)ex_root);
    gvmt_longjump(&gvmt_thread_exception_handler->registers, ex);
}

