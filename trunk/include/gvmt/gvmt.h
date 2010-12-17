#ifndef GVMT_H
#define GVMT_H

#include <inttypes.h>
#include <stdio.h>

#ifndef GVMT_LINKAGE
#error GVMT-linkage header - Use GVMT (not native) compiler.
#endif

#include "gvmt/internal/shared.h"

/* Memory interface */

/** Allocate s bytes of memory on the heap */
GVMT_Object gvmt_malloc(size_t s);

/* Initialise the heap and GC.
 * Size is a (soft) limit on the maximum heap size.
 * Residency is a hint for heap residency. 
 * A zero value for residency will use the default.
 * Must be called before gvmt_malloc() or GC-related functions are called. */
void gvmt_malloc_init(size_t s, float residency);

/** Intrinsics */

/** Intrinsic function for PUSH_CURRENT_STATE */
GVMT_Object gvmt_push_current_state(void);

void* gvmt___untag__(GVMT_Object o);

GVMT_Object gvmt___tag__(void* o);

/** Tagging macros */
#ifndef NDEBUG
# define gvmt_untag(o) ((((intptr_t)gvmt___untag__((GVMT_Object)o)) & 3) ? gvmt___untag__((GVMT_Object)o) : \
                      (assert(0 && "Untagging a reference"), (void*)0))

# define gvmt_tag(o) ((((intptr_t)(o)) & 3) ? gvmt___tag__((void*)(o)) : \
                      (assert(0 && "Tagging a reference"), (GVMT_Object)0))
#else
# define gvmt_tag(o) gvmt___tag__((void*)(o))
# define gvmt_untag(o) gvmt___untag__((GVMT_Object)(o))
#endif

#define gvmt_is_tagged(o) (((intptr_t)gvmt___untag__((GVMT_Object)o)) & 3)

/** Intrinsic function for DISCARD_STATE */
void gvmt_discard_state(void);

/** Intrinsic for RAISE */
void gvmt_raise(GVMT_Object ex);

/** Intrinsic for TRANSFER */
void gvmt_transfer(GVMT_Object ex);

/** Intrinsic for DROP_N */
void gvmt_drop(size_t s);

/** Intrinsic for STACK */
GVMT_Value* gvmt_stack_top(void);

/** Intrinsic for #0 INSERT */
GVMT_Value* gvmt_insert(size_t s);

GVMT_Object gvmt_stack_read_object(GVMT_Value *item);

void gvmt_stack_write_object(GVMT_Value *item, GVMT_Object o);

/** Intrinsic for ALLOCA_I1 */
void *gvmt_alloca(size_t s);

/** Intrinsic for IP */
uint8_t* gvmt_ip(void);

/** Intrinsic for OPCODE */
int gvmt_opcode(void);

/** Intrinsic for NEXT_IP */
uint8_t* gvmt_next_ip(void);

/** Intrinsic for FULLY_INITIALIZED */
void gvmt_fully_initialized(GVMT_Object o);

/* Helper for GVMT_PUSH - Never use directly */
void gvmt_push_x(GVMT_OBJECT(__gvmt_never_use) *z, ...);

/** Macro to push a value to the stack */
#define GVMT_PUSH(x) gvmt_push_x(0, x)

/** Intrinsics for pushing to stack */
void gvmt_push_r(GVMT_Object o, ...);
void gvmt_push_p(void* o, ...);
void gvmt_push_i(intptr_t i, ...);

/** Instrinsics for returning values from interpreter */

void gvmt_ireturn_r(GVMT_Object o, ...);
void gvmt_ireturn_p(void* o, ...);
void gvmt_ireturn_i(intptr_t i, ...);
void gvmt_ireturn_v(void);

/**Intrinsic for #@ (Instruction stream fetch) */
uintptr_t gvmt_fetch(void);

/**Intrinsic for #2@ (Instruction stream two-byte fetch) */
uintptr_t gvmt_fetch2(void);

/**Intrinsic for #4@ (Instruction stream four-byte fetch) */
uintptr_t gvmt_fetch4(void);

/** Intrinsic for FAR_JUMP, only valid in interpreter.
 * Continues interpretation from bytecode at address. */
void gvmt_far_jump(uint8_t* address);

/** Sets the (thread-local) tracing state to s. */
void gvmt_set_tracing(int s);

/** Returns the (thread-local) tracing state. */
int gvmt_get_tracing(void);

typedef intptr_t gvmt_lock_t;

/** Locking intrinsics */
void gvmt_lock(gvmt_lock_t *lock);
void gvmt_unlock(gvmt_lock_t *lock);

/** Lock the mutex at offset in object */
void gvmt_lock_internal(GVMT_Object object, int offset);

/** Unlock the mutex at offset in object */
void gvmt_unlock_internal(GVMT_Object object, int offset);

#define GVMT_LOCK_FIELD(type, obj, field) \
gvmt_lock_internal((GVMT_Object)obj, offsetof(type, field))

#define GVMT_UNLOCK_FIELD(type, obj, field) \
gvmt_lock_internal((GVMT_Object)obj, offsetof(type, field))

/** Force a garbage collection. 
 *  Usually better not to use this and allow the GVMT to decide when to do GC. */
void gvmt_gc_collect(void);

// Initialise compiler - Needs to change to allow multiple compilers.
void gvmt_compiler_init(void);

#define GVMT_TRY(ex) (ex) = (void*)gvmt_push_current_state(); if ((ex) == 0) {
    
#define GVMT_CATCH gvmt_discard_state(); } else { gvmt_discard_state();

#define GVMT_END_TRY }

GSC_Stream make_gsc_stream(FILE* out);

void gvmt_marshal(GVMT_Object object, GSC_Stream m);

typedef void (*gvmt_write_func)(FILE* f, GVMT_Object obj);

uint32_t object_size(GVMT_Object object);

void gvmt_write_ir(void);

void* gvmt_alloca(size_t s);

/** These are constant values, they cannot be assigned to.
 * They have the same value as NULL, but different types */
extern GVMT_Object NULL_R;
extern void* NULL_P;
#define ZERO ((int)NULL_P)

/* Garbage collection interface functions */

/* Informs GC that this obj is finalizable */
void gvmt_gc_finalizable(GVMT_Object obj);

/* Returns a weak reference. Initial value will be NULL. */
void* gvmt_gc_weak_reference(void);

/* Free a weak reference. Weak references cannot be GCed, they must freed explicitly. */
void gvmt_gc_free_weak_reference(void* ref);

/* Read a weak reference. Will be the object last written the weak reference 
 * unless that object has been reclaimed, in which case it will be NULL */ 
#define gvmt_gc_read_weak_reference gvmt_gc_read_root

/* Write to a weak reference. Will be NULL if the object has been reclaimed, 
 * otherwise it will be the object originally used to create the weak reference */ 
#define gvmt_gc_write_weak_reference gvmt_gc_write_root

/* Add a root to the GC root set and initialize it with NULL.
 * The root can be set to NULL or any other object. The object referred to 
 * by the root will kept alive even if referred to by no other objects.
 */
void* gvmt_gc_add_root(void);

/* Pins the object and returns a pointer to it. The pointer remains valid 
 * until the object is collected, after which the pointer must not be used.
 */
void* gvmt_pin(GVMT_Object obj);

/* Treat an address as an object. Must only be applied to objects which
 * have been pinned AND have a root preventing them from being collected.
 */ 
GVMT_Object gvmt_pinned_object(void* pointer);

void gvmt_gc_free_root(void* root);

GVMT_Object gvmt_gc_read_root(void* root);

void gvmt_gc_write_root(void* root, GVMT_Object value);

void* gvmt_gc_add_tls_root(void);

void gvmt_gc_write_tls_root(void* root, GVMT_Object obj);

GVMT_Object gvmt_gc_read_tls_root(void* root);

/** Intrinsic to manually insert GC-safe-point. */
void gvmt_gc_safe_point(void);

/** This does not have any effect on the GC. 
 *  It merely tells the C front-end not to insert any GC-safe-points */
void gvmt_gc_suspend(void);

/** Undoes gvmt_gc_suspend(). 
 *  C Front-end will now insert GC-safe-points as usual */
void gvmt_gc_resume(void);

#define GVMT_NO_GC_BLOCK_BEGIN gvmt_gc_suspend(); {
    
#define GVMT_NO_GC_BLOCK_END } gvmt_gc_resume();

/** Ensure that a reference to object exists at this point */
void gvmt_refer(GVMT_Object o);

// Value that cannot exist in any reference field. used in debug mode
// to validate gvmt_fully_initialized(). Default value is 4.
extern intptr_t gvmt_uninitialised_field;

#endif 
