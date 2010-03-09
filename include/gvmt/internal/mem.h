#ifndef GVMT_INTERNAL_MEM_H
#define GVMT_INTERNAL_MEM_H

#include "gvmt/internal/shared.h"

#ifdef __cplusplus
extern "C" {
#endif 

typedef union gvmt_memory {
    int8_t I1;
    uint8_t U1;
    int16_t I2;
    uint16_t U2;
    int32_t I4;
    uint32_t U4;
    int64_t I8;
    uint64_t U8;
    double F8;
    float F4;
    void *P;
    GVMT_Object R;  
} GVMT_memory;

#if __WORDSIZE == 64

#define GVMT_SET_MEMORY(obj, size, val) \
do { \
    int __i; \
    ((GVMT_memory*)(((char*)obj)+0))->P = 0;  \
    if (size > 8) { \
        ((GVMT_memory*)(((char*)obj)+1))->P = 0;  \
        if (size > 16) { \
            ((GVMT_memory*)(((char*)obj)+2))->P = 0;  \
            for (__i = 24; __i < size; __i+=8) \
                ((GVMT_memory*)(((char*)obj)+__i))->P = 0;  \
        } \
    } \
} while (0)

#else

#define GVMT_SET_MEMORY(obj, size, val) \
do { \
    int __i; \
    ((GVMT_memory*)(((char*)obj)+0))->P = 0;  \
    if (size > 16) { \
        ((GVMT_memory*)(((char*)obj)+4))->P = 0;  \
        ((GVMT_memory*)(((char*)obj)+8))->P = 0;  \
        ((GVMT_memory*)(((char*)obj)+12))->P = 0;  \
        ((GVMT_memory*)(((char*)obj)+16))->P = 0;  \
        for (__i = 20; __i < size; __i+=4) \
            ((GVMT_memory*)(((char*)obj)+__i))->P = 0;  \
    } else { \
        if (size > 8) { \
            ((GVMT_memory*)(((char*)obj)+4))->P = 0;  \
            ((GVMT_memory*)(((char*)obj)+8))->P = 0;  \
            if (size > 12) { \
                ((GVMT_memory*)(((char*)obj)+12))->P = 0;  \
            } \
        } else if (size > 4) { \
            ((GVMT_memory*)(((char*)obj)+4))->P = 0;  \
        } \
    } \
} while (0)

#endif

#ifdef __cplusplus
}
#endif 

#endif
