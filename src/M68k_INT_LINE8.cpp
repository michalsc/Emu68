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

namespace Emu68::M68k::Interpreter {


} // Emu68::M68k::Interpreter

__attribute__((optimize("no-optimize-sibling-calls")))
void INTERPRET_line8(uint32_t opcode)
{
    Emu68::M68k::Interpreter::UNIMPLEMENTED(opcode);
}

