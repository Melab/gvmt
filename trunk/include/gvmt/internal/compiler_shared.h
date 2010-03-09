#ifndef GVMT_COMPILER_SHARED_H
#define GVMT_COMPILER_SHARED_H

#ifdef __cplusplus
extern "C" {
#endif 

enum {
    GVMT_COMPILER_TYPE_V = 0,
    GVMT_COMPILER_TYPE_I1 = 1,
    GVMT_COMPILER_TYPE_I2,
    GVMT_COMPILER_TYPE_I4,
    GVMT_COMPILER_TYPE_I8,
                                  
    GVMT_COMPILER_TYPE_U1 = 5,
    GVMT_COMPILER_TYPE_U2,
    GVMT_COMPILER_TYPE_U4,
    GVMT_COMPILER_TYPE_U8,
          
    GVMT_COMPILER_TYPE_F4 = 11,
    GVMT_COMPILER_TYPE_F8,
          
    GVMT_COMPILER_TYPE_P = 15,
    GVMT_COMPILER_TYPE_R = 19,
};

#ifdef __cplusplus
}
#endif 

#endif
