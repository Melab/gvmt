#include "gvmt/internal/core.h"
#include <malloc.h>
#include <string.h>

/** Repository for machine specific stuff */

// llvm x86 TLS work-around functions

extern GVMT_CALL void* x86_tls_load(intptr_t offset);

extern GVMT_CALL void x86_tls_store(void* value, intptr_t offset);

/*** More advanced versions to overwrite caller sequence ***
 * WARNING - Nasty hack! */

GVMT_CALL void* x86_overwriting_tls_load(intptr_t offset) {
    char* ret_address = __builtin_return_address(0);
    memcpy(ret_address - 5, x86_tls_store, 5);
    return x86_tls_load(offset);
}

GVMT_CALL 
void x86_overwriting_tls_store(void* value, intptr_t offset) {
    char* ret_address = __builtin_return_address(0);
    memcpy(ret_address - 5, x86_tls_store, 5);
    x86_tls_store(value, offset);
}

