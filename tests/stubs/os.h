// Stub for host-side testing — minimal RTOS/OS primitives.
#ifndef OS_H
#define OS_H

#include "type.h"

typedef u32 os_timer_t;

#define OS_TIMER_SECOND  (1000)
#define OS_TIMER_MS      (1)
#define OS_TIMER_MINUTE  (60 * OS_TIMER_SECOND)

static inline os_timer_t OS_TIMER(void)
{
    return (0);
}

static inline os_timer_t os_timer_get(void)
{
    return (0);
}

static inline void OS_DELAY(u32 ms)
{
    (void)ms;
}

#define OS_TASK_YIELD()
#define OS_PUTTEXT(s)     ((void)0)
#define OS_PRINTF(...)    ((void)0)
#define OS_FLUSH()        ((void)0)

// Must be variadic: buf.c calls OS_ASSERT(cond, msg)
#define OS_ASSERT(x, ...) ((void)0)
#define OS_MEM_ALLOC      malloc
#define OS_MEM_FREE       free

// Semaphore slot inside structs — declared but unused on host
#define OS_SEMAPHORE(name)  u32 name
#define OS_SEMAPHORE_INIT(s) ((void)0)
#define OS_SEMAPHORE_TAKE(s) ((void)0)
#define OS_SEMAPHORE_GIVE(s) ((void)0)

#endif
