#ifndef USER_SCAN_H
#define USER_SCAN_H
#include <stdio.h>
#include <inttypes.h>
#include "gvmt/gvmt.h"

#ifndef GVMT_LINKAGE
#error GVMT-linkage header - Use GVMT (not native) compiler.
#endif



/** GVMT_MAX_SHAPE_SIZE is the maximum required length of a shape 
 * vector including the trailing zero */
extern uintptr_t GVMT_MAX_SHAPE_SIZE;

/** User function to handle uncaught exceptions. */
void gvmt_user_uncaught_exception(GVMT_Object ex);

/** User must manage finding marshaling functions for objects *
* Return the (toolkit-provided) function for the type of object 
* If marshaling support is not required, simply return NULL. */
gvmt_write_func gvmt_marshaller_for_object(GVMT_Object object);

/** So that marshalling code can handle arbitary graphs, 
 * each object must have a unique_id. 
* If marshaling support is not required, simply return 0. */
int get_unique_id_for_object(GVMT_Object object);

/** Required for marshalling and helpful for debugging */
void gvmt_readable_name(GVMT_Object obj, char* bufffer);

/** GVMT_MAX_OBJECT_NAME_LENGTH is the maximum possible length of the
 * c-string name of an object(including the terminating NULL). */
extern uintptr_t GVMT_MAX_OBJECT_NAME_LENGTH;

/** The following function must be compiled with the native compiler,
 * not gvmtc */

/** Returns the shape vector for the object */
//int* gvmt_shape(GVMT_Object obj, int* buffer);

#endif
