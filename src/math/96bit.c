#include <stdint.h>

#define unlikely(x)    __builtin_expect(!!(x), 0)

uint64_t Load96bit(uintptr_t __ignore, uintptr_t base)
{
    (void)__ignore;
    
    uint16_t exp = *(uint16_t *)base;
    uint64_t mant = *(uint64_t *)(base + 4);
    uint64_t ret;

    /* Load zero, positive or negative */
    if (unlikely((exp & 0x7fff) == 0 && mant == 0)) {
        return (uint64_t)exp << 48;
    }

    /* Load infinities */
    if (unlikely((exp & 0x7fff) == 0x7fff && (mant & 0x7fffffffffffffffULL) == 0)) {
        return ((uint64_t)exp << 48) & 0xfff0000000000000ULL;
    }

    /* Load NaN */
    if (unlikely((exp & 0x7fff) == 0x7fff && mant != 0)) {
        return ((uint64_t)exp << 48) | 0xffffffffffffULL;
    }

    /* Extract sign of the number */
    if (exp & 0x8000)
        ret = 0x8000000000000000ULL;
    else
        ret = 0;

    // Rescale exponent
    exp = ((exp & 0x7fff) - 0x3fff + 0x3ff) & 0x7ff;
    ret |= (uint64_t)exp << 52;   

    /* Insert mantissa */
    ret |= (mant >> 11) & 0xfffffffffffffULL;

    return ret;
}

void Store96bit(uint64_t value, uintptr_t base)
{
    uint32_t exp = (value & 0x8000000000000000ULL) >> 32;
    uint64_t mant = 0x8000000000000000ULL;

    /* Store zero, positive or negative */
    if (unlikely((value & 0x7fffffffffffffffULL) == 0))
    {
        *(uint64_t *)base = value;
        *(uint32_t *)(base + 8) = 0;
        return;
    }

    /* Store plus/minus infinity */
    if (unlikely((value & 0x7fffffffffffffffULL) == 0x7ff0000000000000ULL))
    {
        *(uint64_t *)base = value | 0x000f000000000000ULL;
        *(uint32_t *)(base + 8) = 0;
        return;
    }

    /* Store NaN */
    if (unlikely((value & 0x7ff0000000000000ULL) == 0x7ff0000000000000ULL && (value & 0x000fffffffffffff) != 0))
    {
        *(uint32_t *)base = ((value >> 32) | 0x000f0000ULL) & 0xffff0000;
        *(uint64_t *)(base + 4) = 0xffffffffffffffffULL;
        return;
    }

    exp |= (((value >> 52) & 0x7ff) - 0x3ff + 0x3fff) << 16;
    mant |= (value & 0x000fffffffffffffULL) << 11;

    *(uint32_t *)base = exp;
    *(uint64_t *)(base + 4) = mant;
}
