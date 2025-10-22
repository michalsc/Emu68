#ifndef _SPINLOCK_H
#define _SPINLOCK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

typedef struct {
    volatile char lock;
} __attribute__((aligned(64))) spinlock_t;

static inline void spinlock_init(spinlock_t *s) {
    s->lock = 0;
}

static inline void spinlock_acquire(spinlock_t *s) {
    __asm__ __volatile__("sevl");
    while (__atomic_test_and_set(&s->lock, __ATOMIC_ACQUIRE))
        __asm__ __volatile__("wfe");
}

static inline void spinlock_release(spinlock_t *s) {
    __atomic_clear(&s->lock, __ATOMIC_RELEASE);
}

static inline bool spinlock_try_acquire(spinlock_t *s) {
    return !__atomic_test_and_set(&s->lock, __ATOMIC_ACQUIRE);
}

#ifdef __cplusplus
}
#endif

#endif /* SPINLOCK_H */
