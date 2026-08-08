#include <cstdint>
#include <arm_neon.h>
#include <array>

#include <emu68/m68k/interpreter.hpp>

#include "RegisterMapping.h"


extern "C" {

void M68K_LoadContext(struct M68KState *ctx);
void M68K_SaveContext(struct M68KState *ctx);

}


namespace Emu68::M68k::Interpreter {

static inline struct M68KState *getCTX()
{
    struct M68KState *ctx;
    __asm__ volatile("mov %0, " CTX_POINTER_ASM:"=r"(ctx));
    return ctx;
}

void BSR(uint32_t opcode)
{
    int32_t bra_off = (int8_t)(opcode & 0xff);
    uint32_t pc = PC + 2;
    uint32_t ret_pc = pc;
    uint32_t ptr = getA<7, uint32_t>() - 4;

    if (bra_off == 0) {
        bra_off = *(int16_t*)(uintptr_t)pc;
        ret_pc += 2;
    } else if (bra_off == -1) {
        bra_off = *(int32_t*)(uintptr_t)pc;
        ret_pc += 4;
    }

    pc += bra_off;
    PC = pc;
    *(uint32_t *)(uintptr_t)ptr = ret_pc;
    setA<7>(ptr);
}

template<uint8_t InstCC>
void Bcc(uint32_t opcode)
{
    int32_t bra_off = (int8_t)(opcode & 0xff);
    uint32_t pc = PC + 2;
    uint32_t next_pc = pc;

    if (bra_off == 0) {
        bra_off = *(int16_t*)(uintptr_t)pc;
        next_pc += 2;
    } else if (bra_off == -1) {
        bra_off = *(int32_t*)(uintptr_t)pc;
        next_pc += 4;
    }

    if (evalCond<InstCC>()) {
        pc += bra_off;
    } else {
        pc = next_pc;
    }

    PC = pc;
}

} // Emu68::M68k::Interpreter

static constexpr std::array<INTERPRET_Function, 16> InsnTable = {
    Emu68::M68k::Interpreter::Bcc<M_CC_T>, // BRA is alias for Bcc with condition TRUE
    Emu68::M68k::Interpreter::BSR,
    Emu68::M68k::Interpreter::Bcc<M_CC_HI>,
    Emu68::M68k::Interpreter::Bcc<M_CC_LS>,
    Emu68::M68k::Interpreter::Bcc<M_CC_CC>,
    Emu68::M68k::Interpreter::Bcc<M_CC_CS>,
    Emu68::M68k::Interpreter::Bcc<M_CC_NE>,
    Emu68::M68k::Interpreter::Bcc<M_CC_EQ>,
    Emu68::M68k::Interpreter::Bcc<M_CC_VC>,
    Emu68::M68k::Interpreter::Bcc<M_CC_VS>,
    Emu68::M68k::Interpreter::Bcc<M_CC_PL>,
    Emu68::M68k::Interpreter::Bcc<M_CC_MI>,
    Emu68::M68k::Interpreter::Bcc<M_CC_GE>,
    Emu68::M68k::Interpreter::Bcc<M_CC_LT>,
    Emu68::M68k::Interpreter::Bcc<M_CC_GT>,
    Emu68::M68k::Interpreter::Bcc<M_CC_LE>
};

__attribute__((optimize("no-optimize-sibling-calls")))
void INTERPRET_line6(uint32_t opcode)
{
    InsnTable[(opcode >> 8) & 15](opcode);
}
