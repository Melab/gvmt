#include "gvmt/internal/core.h"
#include <malloc.h>
#include <stdlib.h>
#include <assert.h>

GVMT_THREAD_LOCAL GvmtExceptionHandler gvmt_exception_stack;
GVMT_THREAD_LOCAL GvmtExceptionHandler gvmt_exception_free_list;

static GvmtExceptionHandler gvmt_pop_handler(void) {
    GvmtExceptionHandler h = gvmt_exception_stack;
    gvmt_exception_stack = h->link;
    h->link = NULL; // Is this necessary?
    return h;
}

void gvmt_pop_and_free_handler(void) {
    GvmtExceptionHandler h = gvmt_pop_handler();
    h->link = gvmt_exception_free_list;
    gvmt_exception_free_list = h;
}

static void gvmt_push_handler(GvmtExceptionHandler handler) {
    handler->link = gvmt_exception_stack;
    gvmt_exception_stack = handler;
}

GvmtExceptionHandler gvmt_create_and_push_handler(void) {
    GvmtExceptionHandler handler = gvmt_exception_free_list;
    if (handler) 
        gvmt_exception_free_list = handler->link;
    else
        handler = malloc(sizeof(struct gvmt_exception_handler));
    gvmt_push_handler(handler);
    return handler;
}

void initialise_exception_stack(void) {
    GvmtExceptionHandler base = malloc(sizeof(struct gvmt_exception_handler));
    base->link = 0;
    gvmt_exception_stack = base;
    gvmt_exception_free_list = NULL;
    GVMT_StackItem* sp = gvmt_stack_pointer;
    gvmt_double_return_t val;
    val.ret = gvmt_setjump(&base->registers, sp);
    if (val.regs.ex) {
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

GVMT_CALL void gvmt_raise_exception(GVMT_Object ex) {
//    gvmt_longjump(&gvmt_exception_stack->registers, ex);
    gvmt_transfer(ex, gvmt_exception_stack->sp);
}

void gvmt_transfer_native(void* ex_root) {
    if (gvmt_thread_non_native == 0)
        // May have to wait for GC to complete.
        gvmt_exit_native();
    assert(gvmt_thread_non_native == 1);
    GVMT_Object ex = *((GVMT_Object*)ex_root);
//    gvmt_longjump(&gvmt_exception_stack->registers, ex);
    gvmt_transfer(ex, gvmt_stack_pointer);
}

void gvmt_raise_native(void* ex_root) {
    if (gvmt_thread_non_native == 0)
        // May have to wait for GC to complete.
        gvmt_exit_native();
    assert(gvmt_thread_non_native == 1);
    GVMT_Object ex = *((GVMT_Object*)ex_root);
//    gvmt_longjump(&gvmt_exception_stack->registers, ex);
    gvmt_transfer(ex, gvmt_exception_stack->sp);
}

