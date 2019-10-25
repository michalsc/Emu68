#ifndef _DUFFCOPY_H
#define _DUFFCOPY_H

#include <stdint.h>

static inline __attribute__((always_inline)) void DuffCopy(uint32_t * restrict to, uint32_t * restrict from, uint32_t count)
{
    register uint32_t n = (count + 7) / 8;
    switch (count % 8) {
    case 0: do { *to++ = *from++; // Fallthrough
    case 7:      *to++ = *from++; // Fallthrough
    case 6:      *to++ = *from++; // Fallthrough
    case 5:      *to++ = *from++; // Fallthrough
    case 4:      *to++ = *from++; // Fallthrough
    case 3:      *to++ = *from++; // Fallthrough
    case 2:      *to++ = *from++; // Fallthrough
    case 1:      *to++ = *from++; // Fallthrough
            } while (--n != 0);
    }
}

#endif /* _DUFFCOPY_H */
