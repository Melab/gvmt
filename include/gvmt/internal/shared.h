#ifndef _GVMT_SHARED_H
#define _GVMT_SHARED_H

#include <inttypes.h>

// If this is redefined, gvmtas.py will need to be modified to match.
#define GVMT_OPCODE(int_name, name) _gvmt_opcode_##int_name##_##name

#define GVMT_OPCODE_LENGTH(int_name, name) _gvmt_opcode_length_##int_name##_##name

#define GVMT_OBJECT(x) struct gvmt_object_##x

#ifdef __cplusplus
extern "C" {
#endif 

extern int gvmt_abort_on_unexpected_parameter_usage;
extern int gvmt_warn_on_unexpected_parameter_usage;
extern double gvmt_heap_residency;

/** GVMT statistics. These are for information and should be treated as read-only.
 * Changing these values will have no effect on the run-time */
extern int gvmt_minor_collections;
extern int gvmt_major_collections;

/** Times are in nanoseconds */
extern int64_t gvmt_minor_collection_time;
extern int64_t gvmt_major_collection_time;
extern int64_t gvmt_total_collection_time;
extern int64_t gvmt_total_compilation_time;
extern int64_t gvmt_total_optimisation_time;
extern int64_t gvmt_total_codegen_time;
/** Heap sizes are in bytes */
extern size_t gvmt_virtual_heap_size;
extern size_t gvmt_real_heap_size;
extern size_t gvmt_nursery_size;
extern size_t gvmt_bytes_passed_to_allocators;
/** This a count of the number of times that 
 *  gvmt_bytes_passed_to_allocators has wrapped */
extern size_t gvmt_passed_to_allocators_wrapped;

size_t gvmt_mature_space_residency(void);

typedef union gvmt_reference_types *GVMT_Object;

typedef struct gsc_stream* GSC_Stream;

extern uintptr_t GVMT_MAX_OBJECT_NAME_LENGTH;

extern uintptr_t GVMT_MAX_SHAPE_SIZE;

/** Returns the stack depth of the current thread */
uintptr_t gvmt_stack_depth(void);

#define GVMT_LOCK_INITIALIZER 1

typedef union gvmt_stack_value {
    int32_t i;
    uint32_t u;
    float f;
    void *p;
    intptr_t j;
    int64_t l;
    uint64_t w;
    double d;
} GVMT_Value;

#ifdef __cplusplus
}
#endif 

#endif
