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

void INTERPRET_BRA(uint32_t opcode)
{
    int32_t bra_off = (int8_t)(opcode & 0xff);
    uint32_t pc = PC + 2;

    if (bra_off == 0)
    {
        bra_off = *(int16_t*)(uintptr_t)pc;
    }
    else if (bra_off == -1)
    {
        bra_off = *(int32_t*)(uintptr_t)pc;
    }

    pc += bra_off;
    PC = pc;
}

void INTERPRET_BSR(uint32_t opcode)
{
    int32_t bra_off = (int8_t)(opcode & 0xff);
    uint32_t pc = PC + 2;
    uint32_t ret_pc = pc;
    uint32_t * a7 = (uint32_t*)(uintptr_t)A7;

    if (bra_off == 0)
    {
        bra_off = *(int16_t*)(uintptr_t)pc;
        ret_pc += 2;
    }
    else if (bra_off == -1)
    {
        bra_off = *(int32_t*)(uintptr_t)pc;
        ret_pc += 4;
    }

    pc += bra_off;
    PC = pc;
    *--a7 = ret_pc;
    A7 = (uint32_t)(uintptr_t)a7;
}

void INTERPRET_BEQ(uint32_t opcode)
{
    uint8_t cc = SR & 0x0f;
    int32_t bra_off = (int8_t)(opcode & 0xff);
    uint32_t pc = PC + 2;
    uint32_t next_pc = pc;

    if (bra_off == 0)
    {
        bra_off = *(int16_t*)(uintptr_t)pc;
        next_pc += 2;
    }
    else if (bra_off == -1)
    {
        bra_off = *(int32_t*)(uintptr_t)pc;
        next_pc += 4;
    }

    if ((cc & SR_Z) != 0) {
        pc += bra_off;
    } else {
        pc = next_pc;
    }

    PC = pc;
}

void INTERPRET_BNE(uint32_t opcode)
{
    uint8_t cc = SR & 0x0f;
    int32_t bra_off = (int8_t)(opcode & 0xff);
    uint32_t pc = PC + 2;
    uint32_t next_pc = pc;

    if (bra_off == 0)
    {
        bra_off = *(int16_t*)(uintptr_t)pc;
        next_pc += 2;
    }
    else if (bra_off == -1)
    {
        bra_off = *(int32_t*)(uintptr_t)pc;
        next_pc += 4;
    }

    if ((cc & SR_Z) == 0) {
        pc += bra_off;
    } else {
        pc = next_pc;
    }

    PC = pc;
}

void INTERPRET_BMI(uint32_t opcode)
{
    uint8_t cc = SR & 0x0f;
    int32_t bra_off = (int8_t)(opcode & 0xff);
    uint32_t pc = PC + 2;
    uint32_t next_pc = pc;

    if (bra_off == 0)
    {
        bra_off = *(int16_t*)(uintptr_t)pc;
        next_pc += 2;
    }
    else if (bra_off == -1)
    {
        bra_off = *(int32_t*)(uintptr_t)pc;
        next_pc += 4;
    }

    if ((cc & SR_N) != 0) {
        pc += bra_off;
    } else {
        pc = next_pc;
    }

    PC = pc;
}

void INTERPRET_BPL(uint32_t opcode)
{
    uint8_t cc = SR & 0x0f;
    int32_t bra_off = (int8_t)(opcode & 0xff);
    uint32_t pc = PC + 2;
    uint32_t next_pc = pc;

    if (bra_off == 0)
    {
        bra_off = *(int16_t*)(uintptr_t)pc;
        next_pc += 2;
    }
    else if (bra_off == -1)
    {
        bra_off = *(int32_t*)(uintptr_t)pc;
        next_pc += 4;
    }

    if ((cc & SR_N) == 0) {
        pc += bra_off;
    } else {
        pc = next_pc;
    }

    PC = pc;
}

void INTERPRET_BCS(uint32_t opcode)
{
    uint8_t cc = SR & 0x0f;
    int32_t bra_off = (int8_t)(opcode & 0xff);
    uint32_t pc = PC + 2;
    uint32_t next_pc = pc;

    if (bra_off == 0)
    {
        bra_off = *(int16_t*)(uintptr_t)pc;
        next_pc += 2;
    }
    else if (bra_off == -1)
    {
        bra_off = *(int32_t*)(uintptr_t)pc;
        next_pc += 4;
    }

    if ((cc & SR_Calt) != 0) {
        pc += bra_off;
    } else {
        pc = next_pc;
    }

    PC = pc;
}

void INTERPRET_BCC(uint32_t opcode)
{
    uint8_t cc = SR & 0x0f;
    int32_t bra_off = (int8_t)(opcode & 0xff);
    uint32_t pc = PC + 2;
    uint32_t next_pc = pc;

    if (bra_off == 0)
    {
        bra_off = *(int16_t*)(uintptr_t)pc;
        next_pc += 2;
    }
    else if (bra_off == -1)
    {
        bra_off = *(int32_t*)(uintptr_t)pc;
        next_pc += 4;
    }

    if ((cc & SR_Calt) == 0) {
        pc += bra_off;
    } else {
        pc = next_pc;
    }

    PC = pc;
}

void INTERPRET_BVS(uint32_t opcode)
{
    uint8_t cc = SR & 0x0f;
    int32_t bra_off = (int8_t)(opcode & 0xff);
    uint32_t pc = PC + 2;
    uint32_t next_pc = pc;

    if (bra_off == 0)
    {
        bra_off = *(int16_t*)(uintptr_t)pc;
        next_pc += 2;
    }
    else if (bra_off == -1)
    {
        bra_off = *(int32_t*)(uintptr_t)pc;
        next_pc += 4;
    }

    if ((cc & SR_Valt) != 0) {
        pc += bra_off;
    } else {
        pc = next_pc;
    }

    PC = pc;
}

void INTERPRET_BVC(uint32_t opcode)
{
    uint8_t cc = SR & 0x0f;
    int32_t bra_off = (int8_t)(opcode & 0xff);
    uint32_t pc = PC + 2;
    uint32_t next_pc = pc;

    if (bra_off == 0)
    {
        bra_off = *(int16_t*)(uintptr_t)pc;
        next_pc += 2;
    }
    else if (bra_off == -1)
    {
        bra_off = *(int32_t*)(uintptr_t)pc;
        next_pc += 4;
    }

    if ((cc & SR_Valt) == 0) {
        pc += bra_off;
    } else {
        pc = next_pc;
    }

    PC = pc;
}

void INTERPRET_BHI(uint32_t opcode)
{
    uint8_t cc = SR & 0x0f;
    int32_t bra_off = (int8_t)(opcode & 0xff);
    uint32_t pc = PC + 2;
    uint32_t next_pc = pc;

    if (bra_off == 0)
    {
        bra_off = *(int16_t*)(uintptr_t)pc;
        next_pc += 2;
    }
    else if (bra_off == -1)
    {
        bra_off = *(int32_t*)(uintptr_t)pc;
        next_pc += 4;
    }

    if ((cc & (SR_Calt | SR_Z)) == 0) {
        pc += bra_off;
    } else {
        pc = next_pc;
    }

    PC = pc;
}

void INTERPRET_BLS(uint32_t opcode)
{
    uint8_t cc = SR & 0x0f;
    int32_t bra_off = (int8_t)(opcode & 0xff);
    uint32_t pc = PC + 2;
    uint32_t next_pc = pc;

    if (bra_off == 0)
    {
        bra_off = *(int16_t*)(uintptr_t)pc;
        next_pc += 2;
    }
    else if (bra_off == -1)
    {
        bra_off = *(int32_t*)(uintptr_t)pc;
        next_pc += 4;
    }

    if ((cc & (SR_Calt | SR_Z)) != 0) {
        pc += bra_off;
    } else {
        pc = next_pc;
    }

    PC = pc;
}

void INTERPRET_BGE(uint32_t opcode)
{
    uint8_t cc = SR & 0x0f;
    int32_t bra_off = (int8_t)(opcode & 0xff);
    uint32_t pc = PC + 2;
    uint32_t next_pc = pc;

    if (bra_off == 0)
    {
        bra_off = *(int16_t*)(uintptr_t)pc;
        next_pc += 2;
    }
    else if (bra_off == -1)
    {
        bra_off = *(int32_t*)(uintptr_t)pc;
        next_pc += 4;
    }

    if (((cc & SR_N) >> 3) == ((cc & SR_Valt) >> 2)) {
        pc += bra_off;
    } else {
        pc = next_pc;
    }

    PC = pc;
}

void INTERPRET_BLT(uint32_t opcode)
{
    uint8_t cc = SR & 0x0f;
    int32_t bra_off = (int8_t)(opcode & 0xff);
    uint32_t pc = PC + 2;
    uint32_t next_pc = pc;

    if (bra_off == 0)
    {
        bra_off = *(int16_t*)(uintptr_t)pc;
        next_pc += 2;
    }
    else if (bra_off == -1)
    {
        bra_off = *(int32_t*)(uintptr_t)pc;
        next_pc += 4;
    }

    if (((cc & SR_N) >> 3) != ((cc & SR_Valt) >> 2)) {
        pc += bra_off;
    } else {
        pc = next_pc;
    }

    PC = pc;
}

void INTERPRET_BGT(uint32_t opcode)
{
    uint8_t cc = SR & 0x0f;
    int32_t bra_off = (int8_t)(opcode & 0xff);
    uint32_t pc = PC + 2;
    uint32_t next_pc = pc;

    if (bra_off == 0)
    {
        bra_off = *(int16_t*)(uintptr_t)pc;
        next_pc += 2;
    }
    else if (bra_off == -1)
    {
        bra_off = *(int32_t*)(uintptr_t)pc;
        next_pc += 4;
    }

    if (((cc & SR_Z) == 0) && (((cc & SR_N) >> 3) == ((cc & SR_Valt) >> 2))) {
        pc += bra_off;
    } else {
        pc = next_pc;
    }

    PC = pc;
}

void INTERPRET_BLE(uint32_t opcode)
{
    uint8_t cc = SR & 0x0f;
    int32_t bra_off = (int8_t)(opcode & 0xff);
    uint32_t pc = PC + 2;
    uint32_t next_pc = pc;

    if (bra_off == 0)
    {
        bra_off = *(int16_t*)(uintptr_t)pc;
        next_pc += 2;
    }
    else if (bra_off == -1)
    {
        bra_off = *(int32_t*)(uintptr_t)pc;
        next_pc += 4;
    }

    if (((cc & SR_Z) != 0) || (((cc & SR_N) >> 3) != ((cc & SR_Valt) >> 2))) {
        pc += bra_off;
    } else {
        pc = next_pc;
    }

    PC = pc;
}

static constexpr std::array<INTERPRET_Function, 16> InsnTable = {
    INTERPRET_BRA,
    INTERPRET_BSR,
    INTERPRET_BHI,
    INTERPRET_BLS,
    INTERPRET_BCC,
    INTERPRET_BCS,
    INTERPRET_BNE,
    INTERPRET_BEQ,
    INTERPRET_BVC,
    INTERPRET_BVS,
    INTERPRET_BPL,
    INTERPRET_BMI,
    INTERPRET_BGE,
    INTERPRET_BLT,
    INTERPRET_BGT,
    INTERPRET_BLE
};

__attribute__((optimize("no-optimize-sibling-calls")))
void INTERPRET_line6(uint32_t opcode)
{
    InsnTable[(opcode >> 8) & 15](opcode);
}
