#pragma once

#include <cstdint>
#include <utility>

namespace Emu68::M68k::Interpreter {

struct BcdResult {
    uint8_t value;
    bool carry;
};

static inline constexpr BcdResult bcdAdd(uint8_t dst, uint8_t src, uint8_t x_in)
{
    uint32_t lo = (uint32_t)(dst & 0x0f) + (src & 0x0f);
    uint32_t hi = (uint32_t)(dst & 0xf0) + (src & 0xf0);   // still nibble-shifted, unmixed

    if (x_in) {
        lo += 1;
    }

    uint32_t sum = lo + hi;

    if (lo > 9) {                      // low-nibble radix correction
        sum += 6;
    }

    uint32_t corrField = sum & 0x3f0;  // bits 4-9 of the (possibly corrected) sum
    bool carry = corrField > 0x90;
    if (carry) {
        sum += 0x60;                   // high radix correction
    }

    return { static_cast<uint8_t>(sum), carry };
}
static inline constexpr BcdResult bcdSub(uint8_t dst, uint8_t src, uint8_t x_in)
{
    uint32_t lo  = (uint32_t)(dst & 0x0f) - (src & 0x0f);
    uint32_t hi  = (uint32_t)(dst & 0xf0) - (src & 0xf0);
    uint32_t raw = (uint32_t)(dst & 0xff) - (src & 0xff);

    if (x_in) {
        lo  -= 1;
        raw -= 1;
    }

    uint32_t trackedRaw = raw;
    uint32_t sum = lo + hi;

    if (lo & 0xf0) {
        sum        -= 6;
        trackedRaw -= 6;
    }

    if (raw & 0x100) {
        sum -= 0x60;
    }

    bool carry = (trackedRaw & 0x300) != 0;

    return { static_cast<uint8_t>(sum), carry };
}

} // namespace Emu68::M68k::Interpreter
