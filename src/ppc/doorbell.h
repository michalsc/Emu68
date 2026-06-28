#ifndef _DOORBELL_H
#define _DOORBELL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    volatile uint32_t val;
} doorbell_t;

static inline void doorbell_init(doorbell_t *d) {
    d->val = 0;
}

static inline uint32_t doorbell_wait(doorbell_t *d) {
    uint32_t msg;

    do {
        while ((msg = __atomic_load_n(&d->val, __ATOMIC_ACQUIRE)) == 0) {
            __asm__ __volatile__("nop"); // low-power spin
        }
        // Try to reset to 0 and claim the message
    } while (!__atomic_compare_exchange_n(&d->val, &msg, 0,
                                          true,
                                          __ATOMIC_ACQUIRE,
                                          __ATOMIC_RELAXED));

    return msg;
}

static inline void doorbell_send(doorbell_t *d, uint32_t msg) {
    // Ensure msg is nonzero
    if (msg == 0)
        return;

    __atomic_store_n(&d->val, msg, __ATOMIC_RELEASE);
}

#endif /* _DOORBELL_H */
