#include <cstdint>
#include <arm_neon.h>
#include <array>

#include <emu68/m68k/interpreter.hpp>

#include "RegisterMapping.h"

#define _REGLOCK_H
extern "C" {
    #include "M68k.h"
    #include "support.h"
}

#define INTERPRET_MOVEQ_Dn(dn) \
void INTERPRET_MOVEQ_D##dn(uint32_t opcode) \
{ \
    uint32_t sr = SR & 0xfff0; \
    int32_t value = (int8_t)(opcode & 0xff); \
    D##dn = value; \
    if (value == 0) { \
        sr |= SR_Z; \
    } else if (value < 0) { \
        sr |= SR_N; \
    } \
    SR = sr; \
    PC += 2; \
}

INTERPRET_MOVEQ_Dn(0);
INTERPRET_MOVEQ_Dn(1);
INTERPRET_MOVEQ_Dn(2);
INTERPRET_MOVEQ_Dn(3);
INTERPRET_MOVEQ_Dn(4);
INTERPRET_MOVEQ_Dn(5);
INTERPRET_MOVEQ_Dn(6);
INTERPRET_MOVEQ_Dn(7);

static constexpr std::array<INTERPRET_Function, 16> InsnTable = {
    INTERPRET_MOVEQ_D0,
    INTERPRET_UNIMPLEMENTED,
    INTERPRET_MOVEQ_D1,
    INTERPRET_UNIMPLEMENTED,
    INTERPRET_MOVEQ_D2,
    INTERPRET_UNIMPLEMENTED,
    INTERPRET_MOVEQ_D3,
    INTERPRET_UNIMPLEMENTED,
    INTERPRET_MOVEQ_D4,
    INTERPRET_UNIMPLEMENTED,
    INTERPRET_MOVEQ_D5,
    INTERPRET_UNIMPLEMENTED,
    INTERPRET_MOVEQ_D6,
    INTERPRET_UNIMPLEMENTED,
    INTERPRET_MOVEQ_D7,
    INTERPRET_UNIMPLEMENTED
};

__attribute__((optimize("no-optimize-sibling-calls")))
void INTERPRET_line7(uint32_t opcode)
{
    InsnTable[(opcode >> 8) & 15](opcode);
}
