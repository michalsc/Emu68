#include <cstdint>
#include <arm_neon.h>
#include <array>

#include <emu68/m68k/interpreter.hpp>

#include "RegisterMapping.h"

namespace Emu68::M68k::Interpreter {

template<uint8_t Dn>
void MOVEQ(uint32_t opcode)
{
    uint32_t sr = SR & 0xfff0;
    LONG value = (BYTE)(opcode & 0xff);
    setD<Dn, LONG>(value);
    if (value == 0) {
        sr |= SR_Z;
    } else if (value < 0) {
        sr |= SR_N;
    }
    SR = sr;
    PC += 2;
}

} // Emu68::M68k::Interpreter

static constexpr std::array<INTERPRET_Function, 16> InsnTable = {
    Emu68::M68k::Interpreter::MOVEQ<0>,
    Emu68::M68k::Interpreter::ILLEGAL,
    Emu68::M68k::Interpreter::MOVEQ<1>,
    Emu68::M68k::Interpreter::ILLEGAL,
    Emu68::M68k::Interpreter::MOVEQ<2>,
    Emu68::M68k::Interpreter::ILLEGAL,
    Emu68::M68k::Interpreter::MOVEQ<3>,
    Emu68::M68k::Interpreter::ILLEGAL,
    Emu68::M68k::Interpreter::MOVEQ<4>,
    Emu68::M68k::Interpreter::ILLEGAL,
    Emu68::M68k::Interpreter::MOVEQ<5>,
    Emu68::M68k::Interpreter::ILLEGAL,
    Emu68::M68k::Interpreter::MOVEQ<6>,
    Emu68::M68k::Interpreter::ILLEGAL,
    Emu68::M68k::Interpreter::MOVEQ<7>,
    Emu68::M68k::Interpreter::ILLEGAL
};

__attribute__((optimize("no-optimize-sibling-calls")))
void INTERPRET_line7(uint32_t opcode)
{
    InsnTable[(opcode >> 8) & 15](opcode);
}
