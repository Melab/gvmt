#ifndef GVMT_COMPILER_H
#define GVMT_COMPILER_H

#include "gvmt/gvmt.h"
#include "gvmt/internal/compiler_shared.h"

void gvmt_compile(uint8_t* begin, uint8_t* end, int ret_type, char* name);

void* gvmt_compile_jit(uint8_t* begin, uint8_t* end, int ret_type, char* name);

void gvmt_write_ir(void);

#endif
