#ifndef _GR_ATOMIC_H__
#define _GR_ATOMIC_H__

#include <pthread.h>

typedef int64_t                        GR_AtomicInt;
typedef uint64_t                       GR_AtomicUint;
typedef volatile GR_AtomicUint         GR_Atomic;

#define GR_AtomicCmpSet(lock, old, set)                                    \
    __sync_bool_compare_and_swap(lock, old, set)

#define GR_AtomicFetchAdd(value, add)                                      \
    __sync_fetch_and_add(value, add)


#endif
