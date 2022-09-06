#ifndef __LOCK_H__
#define __LOCK_H__

#if defined(__STDC__VERSION__) && __STDC_VERSION__ >= 201101L
#include <stdatomic.h>

typedef _Atomic int lock_t;

//   we use setLock(), release() in circumstances where we don't care of race.
// program use setLock() to set a lock without blocking, then use spin*() to 
// wait for it to release. here we ensure that the wait process won't end 
// before release() is called. the race conditions will result in at most one
// redundant isLocked() call.
//   we use lock(), release() in circumstances where we care of race. it's 
// actually a standard spin lock.

static inline void release(lock_t *plock)
{
    //atomic_thread_fence(memory_order_seq_cst);
    //atomic_signal_fence(memory_order_seq_cst);

    atomic_store_explicit(plock, 0, memory_order_release);
}
static inline int tryLock(lock_t *plock)
{
    lock_t ex = 0, de = 1;
    return atomic_compare_exchange_strong_explicit(plock, &ex, de,
        memory_order_acquire, memory_order_acquire);
}
static inline void lock(lock_t *plock)
{
    while (!tryLock(plock));
}
static inline int isLocked(lock_t *plock)
{
    //atomic_thread_fence(memory_order_seq_cst);
    //atomic_signal_fence(memory_order_seq_cst);

    return atomic_load_explicit(plock, memory_order_acquire);
}
static inline void setLock(lock_t *plock)
{
    //atomic_thread_fence(memory_order_seq_cst);
    //atomic_signal_fence(memory_order_seq_cst);

    atomic_store_explicit(plock, 1, memory_order_release);
}
static inline int atomicInc(lock_t *lock)
{
    return atomic_fetch_add_explicit(lock, 1, memory_order_relaxed);
}
#else
typedef volatile int lock_t;

//   we use setLock(), release() in circumstances where we don't care of race.
// program use setLock() to set a lock without blocking, then use spin*() to 
// wait for it to release. here we ensure that the wait process won't end 
// before release() is called. the race conditions will result in at most one
// redundant isLocked() call.
//   we use lock(), release() in circumstances where we care of race. it's 
// actually a standard spin lock.

static inline void release(lock_t *plock)
{
    lock_t x = 0;
    __atomic_store(plock, &x, __ATOMIC_RELEASE);
}
static inline int tryLock(lock_t *plock)
{
    lock_t ex = 0, de = 1;
    return __atomic_compare_exchange(plock, &ex, &de, 0,
        __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE);
}
static inline void lock(lock_t *plock)
{
    while (!tryLock(plock));
}
static inline int isLocked(lock_t *plock)
{
    lock_t x;
    __atomic_load(plock, &x, __ATOMIC_ACQUIRE);
    return x;
}
static inline void setLock(lock_t *plock)
{
    int x = 1;
    __atomic_store(plock, &x, __ATOMIC_RELEASE);
}
static inline int atomicInc(lock_t *lock)
{
    return __atomic_fetch_add(lock, 1, __ATOMIC_RELAXED);
}
#endif

static inline void spin(lock_t *plock)
{
    while (isLocked(plock));
}
static inline void spinAND(lock_t *plock1, lock_t *plock2)
{
    while (isLocked(plock1) && isLocked(plock2));
}
static inline void spinAND3(lock_t *plock1, lock_t *plock2, lock_t *plock3)
{
    while (isLocked(plock1) && isLocked(plock2) && isLocked(plock3));
}

#endif