#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <malloc.h>
#include <inttypes.h>
#include "gvmt/marshaller.h"
#include "alloca.h"
#include "gvmt/internal/core.h"

/** Code to mark & scan all objects -- This is NOT for GC. 
   Simple code for dumping image.
   Recursive, so may cause stack overflow for large, deeply-nested images 
*/

struct int_hash_set {
    GVMT_Arena arena;
    int32_t size;
    int32_t capacity;
    int32_t load;
    int32_t* entries;
};

IntegerHashSet integer_hash_set_new(GVMT_Arena arena);

static int32_t hash(int32_t x) {
    // REPLACE THIS WITH A PROPER HASH
    return x * 5 + 1;
}

int32_t integer_hash_set_contains(IntegerHashSet set, int32_t x) {
    assert(x);
    int32_t k = x;
    do {
        if (set->entries[k & (set->capacity-1)] == x)
            return 1;
        if (set->entries[k & (set->capacity-1)] == 0)
            return 0;
        k = hash(k);
    } while (1);
}

static void integer_hash_set_insert(IntegerHashSet set, int32_t x) {   
    int32_t k = x;
    do {
        if (set->entries[k & (set->capacity-1)] == x)
            return;
        if (set->entries[k & (set->capacity-1)] == 0) {
            set->entries[k & (set->capacity-1)] = x;
            set->size ++;
            return;
        }
        k = hash(k);
    } while (1);
}

static void resize(IntegerHashSet set) {
    int32_t* old_entries = set->entries;
    int32_t old_capacity = set->capacity;
    set->capacity *= 2;
    set->size = 0;
    set->load = set->capacity * 2 / 3;
    set->entries = gvmt_allocate(sizeof(int32_t) * set->capacity, set->arena);
    int i;
    for (i = 0; i < old_capacity; i++)
        if (old_entries[i])
            integer_hash_set_insert(set, old_entries[i]);
}


void integer_hash_set_add(IntegerHashSet set, int32_t x) {   
    assert(x);
    if (set->load == set->size)
        resize(set);
    integer_hash_set_insert(set, x);
}

IntegerHashSet integer_hash_set_new(GVMT_Arena arena) {
    IntegerHashSet set = gvmt_allocate(sizeof(struct int_hash_set), arena);
    set->arena = arena;
    set->size = 0;
    set->load = 256 * 2 / 3;
    set->capacity = 256;
    set->entries = gvmt_allocate(sizeof(int32_t) * 256, arena);
    return set;
}

/* This function is NOT part of the API */
GVMT_LINKAGE_2(_gvmt_fetch_shape, GVMT_Object object, int* buffer)
    int* shape = gvmt_shape(object, buffer);
    // Need to copy shape, just in case object-type is moved.
    if (shape != buffer) {
        int *to = buffer;
        do {
            *to++ = *shape;
        } while (*shape++);
    }
    GVMT_RETURN_V;
}

uint32_t size_from_shape(int* shape) {
    uint32_t size = 0;
    while(*shape) {
        if (*shape > 0)
            size += *shape;
        else
            size -= *shape;
        shape++;
    }
    return size * sizeof(void*);
}

/** GVMT signature is uint32_t object_size(GVMT_Object object); */

GVMT_LINKAGE_1(object_size, GVMT_Object object)
   int buffer[GVMT_MAX_SHAPE_SIZE];
   GVMT_RETURN_I(size_from_shape(gvmt_shape(object, buffer)));
}

void gsc_write_uint16_t(FILE* out, uint16_t val) {
    fprintf(out, "uint16 %d\n", val);
}

void gsc_write_uint8_t(FILE* out, uint8_t val) {
    fprintf(out, "uint8 %d\n", val);
}

void gsc_write_uint64_t(FILE* out, uint64_t val) {
    fprintf(out, "uint64 0x%x%x\n", (uint32_t)(val >> 32), (uint32_t)val);
}

void gsc_write_float(FILE* out, float val) {
    fprintf(out, "float %g\n", val);
}

void gsc_write_double(FILE* out, double val) {
    fprintf(out, "double %g\n", val);
}

void gsc_write_uint32_t(FILE* out, uint32_t val) {
    fprintf(out, "uint32 %d\n", val);
}

void gsc_write_int32_t(FILE* out, int32_t val) {
    fprintf(out, "int32 %d\n", val);
}

void gsc_write_int16_t(FILE* out, int16_t val) {
    fprintf(out, "int16 %d\n", val);
}

void gsc_write_int8_t(FILE* out, int8_t val) {
    fprintf(out, "int8 %d\n", val);
}

void gsc_write_int64_t(FILE* out, int64_t val) {
    fprintf(out, "uint64 0x%x%x\n", (int32_t)(val >> 32), (int32_t)val);
}

void gsc_address(FILE* out, char* addr) {
    fprintf(out, "address %s\n", addr);
}

void gsc_string(FILE* out, char* text) {
    int i, n = strlen(text);
    fprintf(out, "string \"");
    for (i = 0; i <= n; i++) {
        unsigned char c = (unsigned char)text[i];
        if (((unsigned char)c) > 126)
            fprintf(out, "\\%o", c);
        else if (c < 8)
            fprintf(out, "\\00%o", c);
        else if (c < 32)
            fprintf(out, "\\0%o", c);
        else
            putc(c, out);
    }
    fprintf(out, "\"\n");
}

void gsc_footer(FILE* out) {
    fprintf(out, ".end\n");
}

GSC_Stream make_gsc_stream(FILE* out) {
     GSC_Stream s = malloc(sizeof(struct gsc_stream));  
     s->marked_set = integer_hash_set_new(gvmt_new_arena());
     s->out = out;
     return s;
}

