#pragma once

#include <cstdint>
#include <utility>

namespace Emu68::M68k::Interpreter {

struct BcdResult {
    uint8_t value;
    bool carry;
};

// dst + src + x_in, decimal-corrected. Mirrors EMIT_ABCD_reg/EMIT_ABCD_mem.
static inline constexpr BcdResult bcdAdd(uint8_t dst, uint8_t src, uint8_t x_in)
{
    uint32_t lo = (dst & 0x0f) + (src & 0x0f) + x_in;
    uint32_t sum = lo + ((dst & 0xf0) + (src & 0xf0));

    if (lo > 9) {
        sum += 6;
    }

    bool carry = (sum & 0x3f0) > 0x90;
    if (carry) {
        sum += 0x60;
    }

    return { static_cast<uint8_t>(sum), carry };
}

// dst - src - x_in, decimal-corrected. Used directly by SBCD, and by NBCD as bcdSub(0, v, x_in).
static inline constexpr BcdResult bcdSub(uint8_t dst, uint8_t src, uint8_t x_in)
{
    int32_t lo = (dst & 0x0f) - (src & 0x0f) - x_in;
    int32_t sum = lo + ((dst & 0xf0) - (src & 0xf0));

    if (lo < 0) {
        sum -= 6;
    }

    bool borrow = sum < 0;
    if (borrow) {
        sum -= 0x60;
    }

    return { static_cast<uint8_t>(sum), borrow };
}

} // namespace Emu68::M68k::Interpreter
