.code

GC_MALLOC_INLINE[private]:
NAME(0,"size") TSTORE_U4(0) 
TLOAD_U4(0) 3 ADD_U4 -4 AND_U4 NAME(2,"asize") TSTORE_U4(2) 
__GC_FREE_POINTER_LOAD NAME(3,"fp") TSTORE_I4(3) 
TLOAD_I4(3) NEG_I4 TSTORE_I4(6) TLOAD_U4(2) TLOAD_I4(6) 4095 AND_I4 LE_U4 BRANCH_T(0)
TLOAD_U4(2) TLOAD_I4(6) 16383 AND_I4 GT_U4 BRANCH_T(1) 
TLOAD_U4(2) 4095 LE_U4 BRANCH_T(0) 
TARGET(1) 
TLOAD_U4(2) GC_MALLOC_CALL NAME(4,"result") TSTORE_R(4) 
HOP(2) TARGET(0) 
TLOAD_I4(3) TSTORE_R(4) 
TLOAD_U4(2) TLOAD_R(4) ADD_P __GC_FREE_POINTER_STORE  
TARGET(2) 
TLOAD_R(4) TLOAD_U4(0) __ZERO_MEMORY
TLOAD_R(4);

GC_SAFE_INLINE[private]:
    ADDR(gvmt_gc_waiting) PLOAD_I1 IF GC_SAFE_CALL ENDIF 
;

GC_ALLOC_ONLY_INLINE[private]:
NAME(0,"size") TSTORE_U4(0) 
TLOAD_U4(0) 3 ADD_U4 -4 AND_U4 NAME(2,"asize") TSTORE_U4(2) 
__GC_FREE_POINTER_LOAD NAME(3,"fp") TSTORE_I4(3) 
TLOAD_I4(3) NEG_I4 TSTORE_I4(6) TLOAD_U4(2) TLOAD_I4(6) 4095 AND_I4 LE_U4 BRANCH_T(0)
TLOAD_U4(2) TLOAD_I4(6) 16383 AND_I4 GT_U4 BRANCH_T(1) 
TLOAD_U4(2) 4095 LE_U4 BRANCH_T(0) 
TARGET(1) 
TLOAD_U4(2) GC_MALLOC_CALL NAME(4,"result") TSTORE_R(4) 
HOP(2) TARGET(0) 
TLOAD_I4(3) TSTORE_R(4) 
TLOAD_U4(2) TLOAD_R(4) ADD_P __GC_FREE_POINTER_STORE
TARGET(2)
TLOAD_R(4);



// No write barrier.
