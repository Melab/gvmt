.code

GC_MALLOC_INLINE[private]:
NAME(0,"size") TSTORE_I4(0)
TLOAD_I4(0) 3 ADD_I4 -4 AND_I4 NAME(4,"asize") TSTORE_I4(4) 
__GC_FREE_POINTER_LOAD NAME(2,"result") TSTORE_R(2) 
TLOAD_I4(4) TLOAD_R(2) ADD_P NAME(3,"new_free") TSTORE_R(3) 
TLOAD_R(3) __GC_FREE_POINTER_STORE                       
TLOAD_R(3) __GC_LIMIT_POINTER_LOAD LT_U4 BRANCH_T(0) 
TLOAD_I4(0) GC_MALLOC_CALL TSTORE_R(2) TARGET(0) 
TLOAD_R(2) TLOAD_I4(0) __ZERO_MEMORY
TLOAD_R(2);

GC_SAFE_INLINE[private]:
    ADDR(gvmt_gc_waiting) PLOAD_I1 IF GC_SAFE_CALL ENDIF 
;

GC_ALLOC_ONLY_INLINE[private]:
NAME(0,"size") TSTORE_I4(0)
TLOAD_I4(0) 3 ADD_I4 -4 AND_I4 NAME(4,"asize") TSTORE_I4(4) 
__GC_FREE_POINTER_LOAD NAME(2,"result") TSTORE_R(2) 
TLOAD_I4(4) TLOAD_R(2) ADD_P NAME(3,"new_free") TSTORE_R(3) 
TLOAD_R(3) __GC_FREE_POINTER_STORE                       
TLOAD_R(3) __GC_LIMIT_POINTER_LOAD LT_U4 BRANCH_T(0) 
TLOAD_I4(0) GC_MALLOC_CALL TSTORE_R(2) TARGET(0) 
TLOAD_R(2);

