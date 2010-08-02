#ifndef GVMT_INTERNAL_H
#define GVMT_INTERNAL_H

#include <inttypes.h>
#include <stddef.h>
#include <assert.h>

#include "gvmt/internal/shared.h"
#include "gvmt/internal/mem.h"

#ifdef __cplusplus
extern "C" {
#endif 

#ifdef GVMT_LINKAGE
#  error Native header - Use native (not GVMT) compiler.
#endif

#include "gvmt/internal/arch.h"

#if __WORDSIZE == 64

typedef union gvmt_stack_item {
    int64_t i;
    uint64_t u;
    double f;
    void *p;
    GVMT_Object o;
} GVMT_StackItem;

#else

typedef union gvmt_stack_item {
    int32_t i;
    uint32_t u;
    float f;
    void *p;
    GVMT_Object o;
} GVMT_StackItem;

typedef union gvmt_double_stack_item {
    int64_t i;
    uint64_t u;
    double f;
    void* w[2];
} GVMT_DoubleStackItem;

#endif

GVMT_CALL int64_t gvmt_setjump(struct gvmt_registers* state, GVMT_StackItem* sp);

GVMT_NO_RETURN GVMT_CALL void gvmt_transfer(void* ex, GVMT_StackItem* sp);

struct gvmt_exception_handler {
    struct gvmt_registers registers;
    GVMT_StackItem* sp;
    uint8_t* ip;  // INTERPRETER ip.
    struct gvmt_exception_handler* link;
    void *pad; // ? 
};

void initialise_exception_stack(void);

typedef struct gvmt_exception_handler *GvmtExceptionHandler;

typedef struct gvmt_frame* GVMT_Frame;

extern GVMT_THREAD_LOCAL GvmtExceptionHandler gvmt_exception_stack;
extern GVMT_THREAD_LOCAL GvmtExceptionHandler gvmt_exception_free_list;

extern GVMT_THREAD_LOCAL GVMT_Frame gvmt_frame_pointer;
extern GVMT_THREAD_LOCAL GVMT_StackItem* gvmt_stack_pointer; // Evaluation stack pointer

extern GVMT_THREAD_LOCAL GVMT_StackItem* gvmt_stack_base;
extern GVMT_THREAD_LOCAL GVMT_StackItem* gvmt_stack_limit;
 
extern GVMT_THREAD_LOCAL GVMT_StackItem* gvmt_gc_free_pointer;
extern GVMT_THREAD_LOCAL GVMT_StackItem* gvmt_gc_limit_pointer;

extern GVMT_THREAD_LOCAL int32_t gvmt_thread_id;
extern GVMT_THREAD_LOCAL int gvmt_thread_non_native;

struct gvmt_frame {
    struct gvmt_frame* previous;
//    intptr_t ip;
    uintptr_t count;
    GVMT_Object refs[0];
};

typedef GVMT_CALL 
GVMT_StackItem* (*gvmt_funcptr)(GVMT_StackItem* sp, GVMT_Frame fp);

typedef int32_t (*gvmt_native_funcptr_I4)(void* p0, ...);
typedef float (*gvmt_native_funcptr_F4)(void* p0, ...);
typedef uint32_t (*gvmt_native_funcptr_U4)(void* p0, ...);
typedef uint64_t (*gvmt_native_funcptr_U8)(void* p0, ...);
typedef int64_t (*gvmt_native_funcptr_I8)(void* p0, ...);
typedef double (*gvmt_native_funcptr_F8)(void* p0, ...);
typedef void (*gvmt_native_funcptr_V)(void* p0, ...);
typedef void* (*gvmt_native_funcptr_P)(void* p0, ...);

typedef int32_t (*gvmt_native_funcptr0_I4)(void);
typedef float (*gvmt_native_funcptr0_F4)(void);
typedef int64_t (*gvmt_native_funcptr0_I8)(void);
typedef uint32_t (*gvmt_native_funcptr0_U4)(void);
typedef uint64_t (*gvmt_native_funcptr0_U8)(void);
typedef double (*gvmt_native_funcptr0_F8)(void);
typedef void (*gvmt_native_funcptr0_V)(void);
typedef void* (*gvmt_native_funcptr0_P)(void);

extern char* gvmt_gc_name;

#ifndef NDEBUG
extern char* gvmt_return_type_names[];
#endif

GVMT_Object gvmt_set_handler(void);

/** Pushes a new <emph>unitialised</emph> handler to the exception handler 
 *  Use gvmt_sejump to initialise handler. The effect of calling gvmt_raise()
 * are undefined until gvmt_setjump() is called */
GvmtExceptionHandler gvmt_create_and_push_handler(void);
void gvmt_pop_and_free_handler(void);

#define GVMT_LINKAGE_0(proc) GVMT_CALL GVMT_StackItem* proc(GVMT_StackItem* gvmt_sp, GVMT_Frame fp) { 
#define GVMT_LINKAGE_1(proc, p0) GVMT_CALL GVMT_StackItem* proc(GVMT_StackItem* gvmt_sp, GVMT_Frame fp) { p0 = gvmt_sp[0].p; gvmt_sp += 1;
#define GVMT_LINKAGE_2(proc, p0, p1) GVMT_CALL GVMT_StackItem* proc(GVMT_StackItem* gvmt_sp, GVMT_Frame fp) { p0 = gvmt_sp[0].p; p1 = gvmt_sp[1].p; gvmt_sp += 2; 
#define GVMT_LINKAGE_3(proc, p0, p1, p2) GVMT_CALL GVMT_StackItem* proc(GVMT_StackItem* gvmt_sp, GVMT_Frame fp) { p0 = gvmt_sp[0].p; p1 = gvmt_sp[1].p; p2 = gvmt_sp[2]; gvmt_sp += 3;
#define GVMT_RETURN_I(x) gvmt_sp[-1].i = (intptr_t)(x); return gvmt_sp-1;
#define GVMT_RETURN_P(x) gvmt_sp[-1].p = (void*)(x); return gvmt_sp-1;
#define GVMT_RETURN_R(x) gvmt_sp[-1].o = (GVMT_Object)(x); return gvmt_sp-1;
#define GVMT_RETURN_V return gvmt_sp;

extern int gvmt_gc_waiting;
void gvmt_gc_safe_point(GVMT_StackItem* sp, GVMT_Frame fp);

extern void* _gvmt_global_symbols;


GVMT_StackItem* gvmt_exit_native(void); 
GVMT_CALL void gvmt_enter_native(GVMT_StackItem* sp, GVMT_Frame fp);

GVMT_CALL GVMT_Object gvmt_fast_allocate(size_t size);

void inform_gc_new_stack(void);

GVMT_NO_RETURN GVMT_CALL void gvmt_raise_exception(GVMT_Object ex);

void __gvmt_fatal(char*fmt, ...);

void __gvmt_expect(char* file, int line, char* func, int expected, int actual);

void __gvmt_expect_v(char* file, int line, char* func, int expected, int actual);

/** Shape function returns the shape of the specified object.
 * If desired the shape may be written into the buffer, 
 * which will be of at least size GVMT_MAX_SHAPE_SIZE */
GVMT_CALL int *gvmt_shape(GVMT_Object object, int *buffer);

GVMT_CALL unsigned int gvmt_length(GVMT_Object object);

int gvmt_get_shape_at_offset(GVMT_Object object, uintptr_t offset);

int gvmt_object_is_initialised(GVMT_Object object, uintptr_t offset);

void gvmt_fully_initialized_check(GVMT_Object object);

GVMT_CALL void gvmt_save_pointers(GVMT_StackItem* sp, GVMT_Frame fp);

#ifndef NDEBUG
#define RETURN_TYPE_V  1
#define RETURN_TYPE_I4 2
#define RETURN_TYPE_I8 3
#define RETURN_TYPE_F4 4
#define RETURN_TYPE_F8 5
#define RETURN_TYPE_P  6
#define RETURN_TYPE_R  7
extern GVMT_THREAD_LOCAL int gvmt_last_return_type;
#endif

#include <sys/mman.h>

#define get_virtual_memory(l) \
mmap(NULL, l, PROT_READ|PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)

#define free_virtual_memory(p, l) munmap(p, l)

GVMT_CALL void gvmt_fast_lock(intptr_t *lock);
GVMT_CALL void gvmt_fast_unlock(intptr_t *lock);

#define GVMT_LOCKING_UNLOCKED 1
#define GVMT_LOCKING_LOCKED 2

#ifdef __cplusplus
}

#endif 
#endif 

