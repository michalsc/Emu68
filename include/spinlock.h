/*
    Copyright © 2019-2025 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _SPINLOCK_H
#define _SPINLOCK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    volatile char lock;
} __attribute__((aligned(64))) spinlock_t;

static inline void spinlock_init(spinlock_t *s) {
    s->lock = 0;
}

/*
    Wait with the lock address armed in the exclusive monitor (LDAXRB), so the
    owner's releasing store clears the monitor and wakes us from WFE. No SEV is
    needed on release. This is the WFE/exclusive-monitor lock idiom described
    in the Arm Architecture Reference Manual.
*/
static inline void spinlock_acquire(spinlock_t *s) {
    uint32_t tmp;
    __asm__ __volatile__(
        "   sevl                    \n"
        "1: wfe                     \n"
        "2: ldaxrb  %w0, [%1]       \n"
        "   cbnz    %w0, 1b         \n"
        "   stxrb   %w0, %w2, [%1]  \n"
        "   cbnz    %w0, 2b         \n"
        : "=&r"(tmp)
        : "r"(&s->lock), "r"(1)
        : "memory");
}

static inline void spinlock_release(spinlock_t *s) {
    __atomic_clear(&s->lock, __ATOMIC_RELEASE);   /* STLRB wakes monitor-armed waiters */
}

static inline bool spinlock_try_acquire(spinlock_t *s) {
    return !__atomic_test_and_set(&s->lock, __ATOMIC_ACQUIRE);
}

#ifdef __cplusplus
}
#endif

#endif /* SPINLOCK_H */
