#ifndef MARSHALLER_H
#define MARSHALLER_H
#include <inttypes.h>
#include "gvmt/internal/shared.h"
#include "gvmt/arena.h"
#include <stdio.h>

// All calls are treated as native by GVMT - OK for both linkage.

typedef struct int_hash_set * IntegerHashSet;
extern IntegerHashSet integer_hash_set_new(GVMT_Arena arena);
int32_t integer_hash_set_contains(IntegerHashSet set, int32_t x);
void integer_hash_set_add(IntegerHashSet set, int32_t x);

struct gsc_stream {
    IntegerHashSet marked_set;
    FILE* out;
};

void gsc_write_uint16_t(FILE* out, uint16_t val);
void gsc_write_uint8_t(FILE* out, uint8_t val);
void gsc_write_uint64_t(FILE* out, uint64_t val);
void gsc_write_float(FILE* out, float val);
void gsc_write_double(FILE* out, double val);
void gsc_write_uint32_t(FILE* out, uint32_t val);
void gsc_write_int32_t(FILE* out, int32_t val);
void gsc_write_int16_t(FILE* out, int16_t val);
void gsc_write_int8_t(FILE* out, int8_t val);
void gsc_write_int64_t(FILE* out, int64_t val);
void gsc_address(FILE* out, char* addr);
void gsc_string(FILE* out, char* addr);
void gsc_footer(FILE* out);

/** Internal function for marshaling */
void gvmt_write_ref(FILE* out, GVMT_Object object);

#endif
