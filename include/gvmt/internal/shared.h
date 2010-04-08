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
/** Heap size in bytes */
extern int64_t gvmt_heap_size;
/** Times are in nanoseconds */
extern int64_t gvmt_minor_collection_time;
extern int64_t gvmt_major_collection_time;
extern int64_t gvmt_total_collection_time;
extern int64_t gvmt_total_compilation_time;
extern int64_t gvmt_total_optimisation_time;
extern int64_t gvmt_total_codegen_time;

typedef union gvmt_reference_types *GVMT_Object;

typedef struct gsc_stream* GSC_Stream;

extern uintptr_t GVMT_MAX_OBJECT_NAME_LENGTH;

extern uintptr_t GVMT_MAX_SHAPE_SIZE;

/** Returns the stack depth of the current thread */
uintptr_t gvmt_stack_depth(void);

#define GVMT_LOCK_INITIALIZER 1

#ifdef __cplusplus
}
#endif 

#endif
