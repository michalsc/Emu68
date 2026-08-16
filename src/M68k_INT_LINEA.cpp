#include <cstdint>
#include <arm_neon.h>
#include <array>

#include <emu68/m68k/interpreter>

#include "RegisterMapping.h"

namespace Emu68::M68k::Interpreter::LineA {

void ILLEGAL_LINE_A(uint32_t)
{
    raiseException(VECTOR_LINE_A, ExceptionFrameFormat::FORMAT_0, 0, 0);
}

static constexpr std::array<INTERPRET_Function, 4096> buildInsnTable()
{
    std::array<INTERPRET_Function, 4096> table{};
    for (auto& e : table) e = ILLEGAL_LINE_A;
    return table;
}

constexpr auto InsnTable __attribute__((used,aligned(4096),section(".int.jumptable.a"))) = buildInsnTable();

} // Emu68::M68k::Interpreter::LineA
