/*
    Copyright © 2019-2025 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _DOORBELL_H
#define _DOORBELL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct {
    volatile uint32_t val;
} __attribute__((aligned(64))) doorbell_t;

static inline void doorbell_init(doorbell_t *d) {
    d->val = 0;
}

static inline uint32_t doorbell_wait(doorbell_t *d) {
    uint32_t msg, fail;

    __asm__ __volatile__(
        "1: ldaxr   %w0, [%2]       \n"
        "   cbnz    %w0, 2f         \n"
        "   wfe                     \n"
        "   b       1b              \n"
        "2: stxr    %w1, wzr, [%2]  \n"
        "   cbnz    %w1, 1b         \n"
        : "=&r"(msg), "=&r"(fail)
        : "r"(&d->val)
        : "memory");

    return msg;
}

static inline void doorbell_send(doorbell_t *d, uint32_t msg) {
    // Ensure msg is nonzero
    if (msg == 0)
        return;

    __atomic_store_n(&d->val, msg, __ATOMIC_RELEASE);
}

#ifdef __cplusplus
}
#endif

#endif /* _DOORBELL_H */
