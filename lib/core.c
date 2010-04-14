#include <dlfcn.h>
#include "gvmt/internal/core.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <malloc.h>
#include "gvmt/arena.h"
#include <pthread.h>

// Initialise to arbitrary values, VMs should redefine these.
uintptr_t GVMT_MAX_SHAPE_SIZE = 8;
uintptr_t GVMT_MAX_OBJECT_NAME_LENGTH = 64;

static int gvmt_initialised = 0;
// Thread local state:

GVMT_THREAD_LOCAL GVMT_Frame gvmt_frame_pointer;

GVMT_THREAD_LOCAL int32_t gvmt_thread_id;
GVMT_THREAD_LOCAL GVMT_StackItem* gvmt_stack_base;
GVMT_THREAD_LOCAL GVMT_StackItem* gvmt_stack_limit;

GVMT_THREAD_LOCAL GVMT_StackItem* gvmt_gc_free_pointer;
GVMT_THREAD_LOCAL GVMT_StackItem* gvmt_gc_limit_pointer;

GVMT_THREAD_LOCAL GVMT_StackItem* gvmt_stack_pointer;

GVMT_THREAD_LOCAL int gvmt_tracing_state;
GVMT_THREAD_LOCAL int gvmt_thread_non_native;

int gvmt_gc_waiting;

int gvmt_abort_on_unexpected_parameter_usage = 0;
int gvmt_warn_on_unexpected_parameter_usage = 1;                      
int gvmt_minor_collections = 0;
int gvmt_major_collections = 0;
int64_t gvmt_minor_collection_time = 0;
int64_t gvmt_major_collection_time = 0;
int64_t gvmt_total_collection_time = 0;
int64_t gvmt_total_compilation_time = 0;
int64_t gvmt_total_optimisation_time = 0;
int64_t gvmt_total_codegen_time = 0;
int64_t gvmt_heap_size = 0;

GVMT_THREAD_LOCAL int gvmt_last_return_type;
#ifndef NDEBUG
int gvmt_debug_on = 0;
#endif

void* __sp(void) {
    return (void*)gvmt_stack_pointer;   
}

void* __stack_base(void) {
    return (void*)gvmt_stack_base;   
}

int gvmt_tracing_on = 0;

extern GVMT_THREAD_LOCAL GVMT_Arena gvmt_handler_arena;

char* gvmt_return_type_names[] = {
0,
"V ",
"I4",
"I8",
"F4",
"F8",
"P ",
"R "
};

static GVMT_StackItem* push_args(GVMT_StackItem* sp, int pcount, va_list ap) {
    int i;
    sp -= pcount;
    assert(gvmt_initialised);
    for (i = 0; i < pcount; i++) {
        void* v = va_arg(ap, void*);
        sp[i].p = v;
    }
    va_end(ap);
    return sp;
}

typedef GVMT_CALL GVMT_StackItem* (*gvmt_func_ptr)(GVMT_StackItem* sp, GVMT_Frame fp);

static GVMT_StackItem* gvmt_call(GVMT_StackItem* sp, GVMT_Frame fp, gvmt_func_ptr func, int pcount, va_list ap) {
    sp = push_args(sp, pcount, ap);
    return func(sp, fp);
}

uintptr_t gvmt_stack_depth(void) {
    return gvmt_stack_base-gvmt_stack_pointer;
}

#define WORD_SIZE sizeof(void*)
#define DOUBLE_WORD_SIZE  (2 * WORD_SIZE) 

#if HAVE_COMPARE_AND_SWAP

static void set_thread_id(void) {
    static uint32_t thread_id = 0;
    int id = thread_id;
    while (!COMPARE_AND_SWAP(&thread_id, id, id+4))
        id = thread_id;
    gvmt_thread_id = id + 4;
}

#else

static void set_thread_id(void) {
    static uint32_t thread_id = 0;
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&lock);
    thread_id+= 4;
    gvmt_thread_id = thread_id;
    pthread_mutex_unlock(&lock);
}

#endif

static void gvmt_init_thread(size_t stack_space) {
    assert(gvmt_initialised);
    set_thread_id();
    // Round up stack space to multiple of DOUBLE_WORD_SIZE.
    stack_space = (stack_space + DOUBLE_WORD_SIZE -1) & -DOUBLE_WORD_SIZE;
    gvmt_stack_limit = malloc(stack_space);
    if (gvmt_stack_limit == NULL)
        __gvmt_fatal("Cannot allocate memory for new stack\n");
    GVMT_StackItem* sp = gvmt_stack_limit + stack_space/WORD_SIZE; 
// This ensures that stack caching doesn't leave the memory stack less than empty.
    sp[-1].i = 0;
    sp[-2].i = 0;
    sp -= 2;
    gvmt_stack_base = gvmt_stack_pointer = sp;
    gvmt_frame_pointer = 0;
    // Inform GC of this thread, so it can find roots..
    inform_gc_new_stack();
    initialise_exception_stack();
}

extern GVMT_CALL GVMT_StackItem* gvmt_user_uncaught_exception(GVMT_StackItem* gvmt_sp, GVMT_Frame fp);

static void gvmt_start(size_t stack_space, gvmt_func_ptr func, 
                       int pcount, va_list ap) {  
    gvmt_init_thread(stack_space);
    GVMT_StackItem* sp = gvmt_exit_native();
    GvmtExceptionHandler h = gvmt_create_and_push_handler();
    gvmt_double_return_t val;
    val.ret = gvmt_setjump(&h->registers, sp);
    void *ex = val.regs.ex;
    if (ex == NULL) {
        gvmt_call(sp, 0, func, pcount, ap);
    } else {
        //Push the exception
        sp[-1].o = ex;
        sp--;
        // Call user defined thread-specific uncaught exception handler.
        sp = gvmt_user_uncaught_exception(sp, 0);
    }
    // Tell GC this thread is no longer running.
    gvmt_enter_native(sp, 0);
}

// Move to OS file.

struct pthread_start {
   size_t stack_space;
   gvmt_func_ptr func;
   int pcount;
   va_list ap;
};

static void* pthread_start(void* arg) {
    struct pthread_start* s = (struct pthread_start*)arg;
    gvmt_start(s->stack_space, s->func, s->pcount, s->ap);
    return (void*)0;
}

void OS_create_thread(size_t stack_space, gvmt_func_ptr func, int pcount, va_list ap) { 
    struct pthread_start s;
    s.stack_space = stack_space;
    s.func = func;
    s.pcount = pcount;
    s.ap = ap;
    pthread_t p;
    pthread_create(&p, NULL, pthread_start, (void*)&s);
    return;
}
// End OS specific

void gvmt_enter(uintptr_t stack_space, gvmt_func_ptr func, int pcount, ...) {
    va_list ap;
    va_start(ap, pcount);
    gvmt_start(stack_space, func, pcount, ap);
    va_end(ap);
}

void gvmt_reenter(gvmt_func_ptr func, int pcount, ...) {
    va_list ap;
    va_start(ap, pcount);
    GVMT_StackItem *sp = gvmt_exit_native();
    gvmt_call(sp, gvmt_frame_pointer, func, pcount, ap);
    va_end(ap);
}
    
void gvmt_start_machine(uintptr_t stack_space, gvmt_func_ptr func, int pcount, ...) {
    if (gvmt_initialised)
        abort();   
    gvmt_initialised = 1;
    _gvmt_global_symbols = dlopen(0, RTLD_LAZY | RTLD_GLOBAL);
    va_list ap;
    va_start(ap, pcount);
    gvmt_start(stack_space, func, pcount, ap);
    va_end(ap);
}

void gvmt_start_thread(uintptr_t stack_space, gvmt_func_ptr func, int pcount, ...) {
    va_list ap;
    va_start(ap, pcount);
    OS_create_thread(stack_space, func, pcount, ap);
    va_end(ap);
}

int _assert(char *e, char *file, int line) {
	fprintf(stderr, "assertion failed:");
	if (e)
		fprintf(stderr, " %s", e);
	if (file)
		fprintf(stderr, " file %s", file);
	fprintf(stderr, " line %d\n", line);
	fflush(stderr);
	abort();
	return 0;
}

void __gvmt_fatal(char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    abort();
}

void __gvmt_expect(char* file, int line, char* func, int expected, int actual) {
    if (gvmt_warn_on_unexpected_parameter_usage || gvmt_abort_on_unexpected_parameter_usage)
        fprintf(stderr, "%s:%d: warning: Expected %s to use %d parameters, used %d\n", file, line, func, expected, actual);
    if (gvmt_abort_on_unexpected_parameter_usage)
        abort();
}

void __gvmt_expect_v(char* file, int line, char* func, int expected, int actual) {
    if (gvmt_warn_on_unexpected_parameter_usage || gvmt_abort_on_unexpected_parameter_usage)
        fprintf(stderr, "%s:%d: warning: Expected %s to use no more than %d parameters, used %d\n", file, line, func, expected, actual);
    if (gvmt_abort_on_unexpected_parameter_usage)
        abort();
}
//
//void gvmt_check_frame(struct gvmt_frame* frame, struct gvmt_frame* previous) {
//    struct gvmt_frame* tsf = gvmt_thread_current_frame; 
//    if (tsf != frame)
//        abort();
//    if (frame->previous != previous)
//        abort();
//}

int gvmt_object_is_initialised(GVMT_Object object, uintptr_t offset) {
    void** ptr = (void**)object;
    uintptr_t index = 0;
    while (index * sizeof(void*) < offset) {
        if (ptr[index] != NULL)
            return 1;
        index++;
    }
    return 0;
}

int gvmt_get_shape_at_offset(GVMT_Object object, uintptr_t offset) {
    int buffer[GVMT_MAX_SHAPE_SIZE];
    int* shape = gvmt_shape(object, buffer);
    uintptr_t search = 0;
    int index = 0;
    int val = shape[index];
    while (search <= offset) {
        if (val == 0)
            return 0;
        if (val < 0)
            search -= val * sizeof(void*);
        else
            search += val * sizeof(void*);
        index += 1;
        val = shape[index];
    }
    return shape[index-1];
}

void check_frame_stack_integrity(struct gvmt_frame* frame) {
    int depth = 0;
    while (frame) {
        if (frame->count < 0 || frame->count > (1 << 16))
            __gvmt_fatal("count out of range: %d, at depth %d\n", frame->count, depth);
        frame = frame->previous;
        depth++;
    }
}

/** Functions for accessing TLS, for llvm 
 * TO DO - Remove these when the llvm x86 JIT can handle TLS */

void* gvmt_load_free(void) {
    return (void*)gvmt_gc_free_pointer;   
}

void GVMT_CALL gvmt_store_free(void* free) {
    gvmt_gc_free_pointer = (GVMT_StackItem*)free;   
}

void* gvmt_load_limit(void) {
    return (void*)gvmt_gc_limit_pointer;   
}

void GVMT_CALL gvmt_store_limit(void* limit) {
    gvmt_gc_limit_pointer = (GVMT_StackItem*)limit;   
}
//
//void* gvmt_load_frame_pointer(void) {
//    return (void*)gvmt_thread_current_frame; 
//}
//
//void GVMT_CALL gvmt_store_frame_pointer(struct gvmt_frame* frame) {
//    gvmt_thread_current_frame = frame;
//}
//
//void* gvmt_load_sp(void) {
//    return (void*)gvmt_sp;   
//}
//
//void GVMT_CALL gvmt_store_sp(void* sp) {
//    gvmt_sp = (GVMT_StackItem*)sp;   
//}
//

void* gvmt_interpreter_frame(struct gvmt_frame* frame) {
    do {
        if (frame->interpreter)
            return frame;
        frame = frame->previous;
    } while (frame);
    return NULL;
}

void* gvmt_caller_frame(struct gvmt_frame* frame) {
    if (frame) {
        frame = frame->previous;
        while (frame) {
            if (frame->interpreter)
                return frame;
            frame = frame->previous;
        }
    }
    return NULL;
}

intptr_t gvmt_get_tracing(void) {
    return gvmt_tracing_state;  
}

/** Intrinsic for SET_TRACE */
void gvmt_set_tracing(intptr_t s) {
    gvmt_tracing_state = s;  
}

void gvmt_fully_initialized_check(GVMT_Object object) {
    int i, buffer[GVMT_MAX_SHAPE_SIZE];
    int* shape = gvmt_shape(object, buffer);
    intptr_t* field = (intptr_t*)object;
    while (*shape) {
        if (*shape > 0) {   
            for (i = 0; i < *shape; i++) {
                if (field[i] == -1)
                  __gvmt_fatal("Partly uinitialized object");  
            }   
            field += *shape;
        } else {
            field -= *shape;   
        }
        shape++;
    }
}

