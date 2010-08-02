#include <stdlib.h>
#include "gvmt/internal/core.h"
#include <pthread.h>
#include <stdio.h>

typedef struct _gvmt_heavyweight_lock_t {
    pthread_mutex_t lock; // Must hold this mutex to change anything in the HW lock or to release it.
    pthread_cond_t condition; // To signal that a thread has finished wiht the lock.
    struct _gvmt_heavyweight_lock_t* next;
    int waiters; // Number of threads waiting on this HW lock.
    int depth;  // Recursive locking depth
    int thread_id; // Thread that owns this HW lock.
} gvmt_heavyweight_lock_t;

#if HAVE_COMPARE_AND_SWAP

#define ID_MASK 0x3fffc
#define COUNT_MASK 0xfffc0000
#define COUNT_ONE 0x00040000
#define STATUS_MASK 3
#define CONTENDED 0
#define BUSY 3

static GVMT_CALL gvmt_heavyweight_lock_t *make_lock(void) {
    gvmt_heavyweight_lock_t *lock = (gvmt_heavyweight_lock_t *)malloc(sizeof(gvmt_heavyweight_lock_t));
    pthread_mutex_init(&lock->lock, NULL);
    pthread_cond_init(&lock->condition, NULL);
    lock->next = NULL;
    return lock;
}

// Instead of using C&S to modify global list, could have thread-local lists?
static gvmt_heavyweight_lock_t *free_list = NULL;

gvmt_heavyweight_lock_t *allocate_heavyweight_lock(void) {
    gvmt_heavyweight_lock_t *free;
    gvmt_heavyweight_lock_t *next;
    do {
        free = free_list;
        if (free == NULL) {
            return make_lock();
        }
        next = free->next;
    } while (!COMPARE_AND_SWAP(&free_list, free, next));
    return free;
}

static void free_heavyweight_lock(gvmt_heavyweight_lock_t *lock) {
    do {
        gvmt_heavyweight_lock_t *free = free_list;
        lock->next = free;
    } while (!COMPARE_AND_SWAP(&free_list, free, lock));
}

GVMT_CALL static void gvmt_lock_contended(intptr_t *lock) {
    gvmt_heavyweight_lock_t *slow = (gvmt_heavyweight_lock_t *)*lock;
    do {
        // This is slightly inefficient as it saves already saved pointers, but it is correct.
        gvmt_enter_native(gvmt_stack_pointer, gvmt_frame_pointer);
        pthread_mutex_lock(&slow->lock);
        pthread_cond_wait(&slow->condition, &slow->lock);
        if (slow->thread_id == 0) {
            int waiters;
            slow->thread_id = gvmt_thread_id;
            pthread_mutex_unlock(&slow->lock);
            do {
                waiters = slow->waiters;
            } while(!COMPARE_AND_SWAP(&slow->waiters, waiters, waiters-1));
            gvmt_exit_native();
            return;
        }
    } while (1);
}

GVMT_CALL void gvmt_fast_lock(intptr_t *lock) {
    int fast;
    do {
        fast = *lock;
        if (fast == GVMT_LOCKING_UNLOCKED) {
            if (COMPARE_AND_SWAP(lock, GVMT_LOCKING_UNLOCKED, gvmt_thread_id | GVMT_LOCKING_LOCKED))
                return;
        } else if ((fast & STATUS_MASK) == GVMT_LOCKING_LOCKED) {
            if ((fast & ID_MASK) == gvmt_thread_id) {
                if (COMPARE_AND_SWAP(lock, fast, fast + COUNT_ONE))
                    return;
            } else {
                if (COMPARE_AND_SWAP(lock, fast, (fast & (~STATUS_MASK)) | BUSY)) {
                    // Have spin lock   
                     gvmt_heavyweight_lock_t *slow = allocate_heavyweight_lock();
                     slow->thread_id = ID_MASK & fast;
                     slow->depth = COUNT_MASK & fast;
                     slow->waiters = 1;
                     // Release spin lock and set to CONTENDED
                     *lock = (intptr_t)slow;
                     gvmt_lock_contended(lock);
                     return;
                }
            }
        } else if ((fast & STATUS_MASK) == CONTENDED) {
            gvmt_heavyweight_lock_t *slow = (gvmt_heavyweight_lock_t *)fast;
            if (slow->thread_id == gvmt_thread_id) {
                slow->depth++;
                return;
            }
            if (COMPARE_AND_SWAP(lock, fast, (fast & (~STATUS_MASK)) | BUSY)) {
                // Have spin lock  
                gvmt_heavyweight_lock_t *slow = (gvmt_heavyweight_lock_t *)fast;
                int waiters;
                do {
                    waiters = slow->waiters;
                } while(!COMPARE_AND_SWAP(&slow->waiters, waiters, waiters+1));
                // Release spin lock
                *lock = fast;
                gvmt_lock_contended(lock);
                return;
            }
        } else {
            assert((fast & STATUS_MASK) == BUSY);
            // Spin wait
        }
    } while (1);
}

GVMT_CALL static void gvmt_unlock_contended(intptr_t *lock) {
    intptr_t ptr = *lock & -4; // Might be busy as another thread may be incrementing waiters.
    gvmt_heavyweight_lock_t *slow = (gvmt_heavyweight_lock_t *)*lock;
    if (slow->thread_id != gvmt_thread_id) {
        fprintf(stderr, "Illegal attempt to unlock lock owned by another thread\n");
        abort();
    }
    if (slow->depth) {
        slow->depth--;
        return;
    }
    do {
        ptr = (*lock & (~STATUS_MASK)) | CONTENDED;
    } while(!COMPARE_AND_SWAP(lock, ptr, ptr | BUSY));
    // Have spin lock.
    slow->thread_id = 0;
    if (slow->waiters) {
        *lock = ptr;
        pthread_cond_signal(&slow->condition);
    } else {
        *lock = GVMT_LOCKING_UNLOCKED;
        free_heavyweight_lock(slow);
    }
}

GVMT_CALL void gvmt_fast_unlock(intptr_t *lock) {
    int fast;
    do {
        fast = *lock;
        if ((fast & STATUS_MASK) == GVMT_LOCKING_LOCKED) {
            if (fast & COUNT_MASK) {
                if (COMPARE_AND_SWAP(lock, fast, fast - COUNT_ONE)) {
                    return;  
                }
            } else {
                if (COMPARE_AND_SWAP(lock, fast, GVMT_LOCKING_UNLOCKED)) {
                    return;
                }
            }            
        } else if ((fast & STATUS_MASK) == CONTENDED) {
            gvmt_unlock_contended(lock);
            return;
        } else if ((fast & STATUS_MASK) == BUSY) {
            // Spin wait
        } else {
            assert((fast & STATUS_MASK) == GVMT_LOCKING_UNLOCKED);
            assert("Illegal state" && 0);
        }
    } while (1);
}

#else

// To do...

GVMT_CALL void gvmt_fast_lock(intptr_t *lock) {
    assert("To do" & 0);
}

GVMT_CALL void gvmt_fast_unlock(intptr_t *lock) {
    assert("To do" & 0);
}

#endif

