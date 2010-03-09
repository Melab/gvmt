#include <inttypes.h>
#include <stdio.h>

void gvmt_write_member_ref(const char *name, const char *cast, void* x, FILE* c);

void gvmt_write_member_intptr_t(const char *name, intptr_t x, FILE* c) ;

void gvmt_write_member_int8_t(const char *name, int8_t x, FILE* c);

void gvmt_write_member_int16_t(const char *name, int16_t x, FILE* c) ;

void gvmt_write_member_int32_t(const char *name, int32_t x, FILE* c) ;

void gvmt_write_member_uintptr_t(const char *name, uintptr_t x, FILE* c);

void gvmt_write_member_uint8_t(const char *name, uint8_t x, FILE* c) ;

void gvmt_write_member_uint16_t(const char *name, uint16_t x, FILE* c) ;

void gvmt_write_member_uint32_t(const char *name, uint32_t x, FILE* c);

void gvmt_write_member_float(const char *name, float x, FILE* c);

void gvmt_write_member_double(const char *name, double x, FILE* c);

void gvmt_write_member_array_int8_t(unsigned size, const char *name, int8_t* values, FILE* c);

void gvmt_write_member_array_uint8_t(unsigned size, const char *name, uint8_t* values, FILE* c);

void gvmt_write_member_array_uint32_t(unsigned size, const char *name, uint32_t* values, FILE* c);

void gvmt_write_member_array_int32_t(unsigned size, const char *name, int32_t* values, FILE* c);

void gvmt_write_member_array_intptr_t(unsigned size, const char *name, intptr_t* values, FILE* c);

void gvmt_write_member_array_ref(unsigned size, const char *name, const char *cast, void* values, FILE* c);

