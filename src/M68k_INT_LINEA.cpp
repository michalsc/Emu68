#include <cstdint>
#include <arm_neon.h>
#include <array>

#include <emu68/m68k/interpreter.hpp>

#include "RegisterMapping.h"

namespace Emu68::M68k::Interpreter::LineA {

static constexpr std::array<INTERPRET_Function, 4096> buildInsnTable()
{
    std::array<INTERPRET_Function, 4096> table{};
    for (auto& e : table) e = ILLEGAL;
    return table;
}

constexpr auto InsnTable __attribute__((aligned(4096),section(".int.jumptable.a"))) = buildInsnTable();

} // Emu68::M68k::Interpreter::LineA

__attribute__((optimize("no-optimize-sibling-calls")))
void INTERPRET_lineA(uint32_t opcode)
{
    Emu68::M68k::Interpreter::LineA::InsnTable[opcode & 0xfff](opcode);
}
