#ifndef _SPINLOCK_H
#define _SPINLOCK_H

#include <stdbool.h>

typedef struct {
    volatile char lock;
} spinlock_t;

static inline void spinlock_init(spinlock_t *s) {
    s->lock = 0;
}

static inline void spinlock_acquire(spinlock_t *s) {
    while (__atomic_test_and_set(&s->lock, __ATOMIC_ACQUIRE))
        __asm__ __volatile__("or 27,27,27":::"memory");
}

static inline void spinlock_release(spinlock_t *s) {
    __atomic_clear(&s->lock, __ATOMIC_RELEASE);
}

static inline bool spinlock_try_acquire(spinlock_t *s) {
    return !__atomic_test_and_set(&s->lock, __ATOMIC_ACQUIRE);
}

#endif /* SPINLOCK_H */
