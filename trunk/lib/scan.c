#include <stdio.h>
#include <ctype.h>
#include <alloca.h>
#include <assert.h>
#include "gvmt/gvmt.h"
#include "user.h"
#include "gvmt/marshaller.h"

void gvmt_write_ref(FILE* out, GVMT_Object object) {
    char* buf = alloca(GVMT_MAX_OBJECT_NAME_LENGTH);
    if (object == NULL_R) {
        fprintf(out, "%s", "address 0\n");
    } else {
        gvmt_readable_name(object, buf);
        fprintf(out, "address %s\n", buf);
    }
}

void _gvmt_fetch_shape(GVMT_Object object, int* buffer);

void gvmt_marshal(GVMT_Object object, GSC_Stream m) {
    char buf[200];
    int32_t i, id, *shape;
    int offset = 0;
    gvmt_write_func func;
    if (object == NULL_R)
        return;
    id = get_unique_id_for_object(object);
    if (integer_hash_set_contains(m->marked_set, id))
        return;
    integer_hash_set_add(m->marked_set, id);
    func = gvmt_marshaller_for_object(object);
    gvmt_readable_name(object, buf);
    fprintf(m->out, ".public %s\n", buf);
    fprintf(m->out, ".object %s\n", buf);
    func(m->out, object);
    shape = gvmt_alloca(GVMT_MAX_SHAPE_SIZE * sizeof(void*));
    _gvmt_fetch_shape(object, shape);
    while (*shape) {
        if (*shape < 0) {
            offset -= *shape;
        } else {
            for (i = 0; i < *shape; i++) {
                gvmt_marshal(((GVMT_Object*)object)[offset+i], m);
            }
            offset += *shape;
        }
        shape++;        
    }
}

