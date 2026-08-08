#include <cstdint>
#include <arm_neon.h>
#include <array>

#include <emu68/m68k/interpreter.hpp>

#include "RegisterMapping.h"

namespace Emu68::M68k::Interpreter {

} // Emu68::M68k::Interpreter

__attribute__((optimize("no-optimize-sibling-calls")))
void INTERPRET_lineA(uint32_t opcode)
{
    Emu68::M68k::Interpreter::ILLEGAL(opcode);
}

