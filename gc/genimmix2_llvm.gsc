.code

GC_MALLOC_INLINE[private]:
NAME(0,"size") TSTORE_U4(0)
TLOAD_U4(0) GC_MALLOC_FAST TSTORE_R(1) TLOAD_R(1)
BRANCH_T(0) 
TLOAD_U4(0) GC_MALLOC_CALL TSTORE_R(1)
TARGET(0) 
TLOAD_R(1) TLOAD_U4(0) __ZERO_MEMORY
TLOAD_R(1);

GC_SAFE_INLINE[private]:
    ADDR(gvmt_gc_waiting) PLOAD_I4 IF GC_SAFE_CALL ENDIF 
;

GC_WRITE_BARRIER[private]:
NAME(0,"offset") TSTORE_I4(0) NAME(1,"object") TSTORE_R(1)
TLOAD_R(1) 4294443008 AND_U4 NAME(2,"zone") TSTORE_P(2)
TLOAD_R(1) 524287 AND_U4 7 RSH_U4 NAME(3,"card") TSTORE_U4(3)
1 TLOAD_U4(3) TLOAD_P(2) ADD_P PSTORE_U1
TLOAD_R(1) TLOAD_I4(0) RSTORE_R
;

GC_ALLOC_ONLY_INLINE[private]:
NAME(0,"size") TSTORE_U4(0)
TLOAD_U4(0) GC_MALLOC_FAST TSTORE_R(1) TLOAD_R(1)
BRANCH_T(0) 
TLOAD_U4(0) GC_MALLOC_CALL TSTORE_R(1)
TARGET(0) 
TLOAD_R(1);

