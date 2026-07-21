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

register uint32x4_t reg_v19 asm("v19");   // FPSR, FPIAR, FPCR|SR
register uint64x2_t reg_v20 asm("v20");   // INSN_COUNT, CTX
register uint32x4_t reg_v21 asm("v21");   // CACR, USP, ISP, MSP

static const uint64x2_t one_zero = { 0, 1 };

static inline void bumpINSN_COUNT(void)
{
    reg_v20 = vaddq_u64(reg_v20, one_zero);
}

#define INTERPRET_SWAP_Dn(dn) \
void INTERPRET_SWAP_D##dn(uint32_t) \
{ \
    uint32_t sr = SR & 0xfff0; \
    \
    if (unlikely(D##dn == 0)) { \
        sr |= SR_Z; \
    } else { \
        D##dn = (D##dn >> 16) | (D##dn << 16); \
        if (D##dn & 0x80000000) { \
            sr |= SR_N; \
        } \
    } \
    SR = sr; \
    PC += 2; \
}

INTERPRET_SWAP_Dn(0)
INTERPRET_SWAP_Dn(1)
INTERPRET_SWAP_Dn(2)
INTERPRET_SWAP_Dn(3)
INTERPRET_SWAP_Dn(4)
INTERPRET_SWAP_Dn(5)
INTERPRET_SWAP_Dn(6)
INTERPRET_SWAP_Dn(7)

#define INTERPRET_CLR_B_Dn(dn) \
void INTERPRET_CLR_B_D##dn(uint32_t) \
{ \
    uint16_t sr = SR & ~SR_NZVC; \
    D##dn &= 0xffffff00; \
    SR = sr | SR_Z; \
    PC += 2; \
}

INTERPRET_CLR_B_Dn(0)
INTERPRET_CLR_B_Dn(1)
INTERPRET_CLR_B_Dn(2)
INTERPRET_CLR_B_Dn(3)
INTERPRET_CLR_B_Dn(4)
INTERPRET_CLR_B_Dn(5)
INTERPRET_CLR_B_Dn(6)
INTERPRET_CLR_B_Dn(7)

#define INTERPRET_CLR_W_Dn(dn) \
void INTERPRET_CLR_W_D##dn(uint32_t) \
{ \
    uint16_t sr = SR & ~SR_NZVC; \
    D##dn &= 0xffff0000; \
    SR = sr | SR_Z; \
    PC += 2; \
}

INTERPRET_CLR_W_Dn(0)
INTERPRET_CLR_W_Dn(1)
INTERPRET_CLR_W_Dn(2)
INTERPRET_CLR_W_Dn(3)
INTERPRET_CLR_W_Dn(4)
INTERPRET_CLR_W_Dn(5)
INTERPRET_CLR_W_Dn(6)
INTERPRET_CLR_W_Dn(7)

#define INTERPRET_CLR_L_Dn(dn) \
void INTERPRET_CLR_L_D##dn(uint32_t) \
{ \
    uint16_t sr = SR & ~SR_NZVC; \
    D##dn = 0; \
    SR = sr | SR_Z; \
    PC += 2; \
}

INTERPRET_CLR_L_Dn(0)
INTERPRET_CLR_L_Dn(1)
INTERPRET_CLR_L_Dn(2)
INTERPRET_CLR_L_Dn(3)
INTERPRET_CLR_L_Dn(4)
INTERPRET_CLR_L_Dn(5)
INTERPRET_CLR_L_Dn(6)
INTERPRET_CLR_L_Dn(7)


#define INTERPRET_CLR_B_An_Addr(dn) \
void INTERPRET_CLR_B_A##dn##_Addr(uint32_t) \
{ \
    uint16_t sr = SR & ~SR_NZVC; \
    *(uint8_t*)(uintptr_t)A##dn = 0; \
    SR = sr | SR_Z; \
    PC += 2; \
}

INTERPRET_CLR_B_An_Addr(0)
INTERPRET_CLR_B_An_Addr(1)
INTERPRET_CLR_B_An_Addr(2)
INTERPRET_CLR_B_An_Addr(3)
INTERPRET_CLR_B_An_Addr(4)
INTERPRET_CLR_B_An_Addr(5)
INTERPRET_CLR_B_An_Addr(6)
INTERPRET_CLR_B_An_Addr(7)

#define INTERPRET_CLR_W_An_Addr(dn) \
void INTERPRET_CLR_W_A##dn##_Addr(uint32_t) \
{ \
    uint16_t sr = SR & ~SR_NZVC; \
    *(uint16_t*)(uintptr_t)A##dn = 0; \
    SR = sr | SR_Z; \
    PC += 2; \
}

INTERPRET_CLR_W_An_Addr(0)
INTERPRET_CLR_W_An_Addr(1)
INTERPRET_CLR_W_An_Addr(2)
INTERPRET_CLR_W_An_Addr(3)
INTERPRET_CLR_W_An_Addr(4)
INTERPRET_CLR_W_An_Addr(5)
INTERPRET_CLR_W_An_Addr(6)
INTERPRET_CLR_W_An_Addr(7)

#define INTERPRET_CLR_L_An_Addr(dn) \
void INTERPRET_CLR_L_A##dn##_Addr(uint32_t) \
{ \
    uint16_t sr = SR & ~SR_NZVC; \
    *(uint32_t*)(uintptr_t)A##dn = 0; \
    SR = sr | SR_Z; \
    PC += 2; \
}

INTERPRET_CLR_L_An_Addr(0)
INTERPRET_CLR_L_An_Addr(1)
INTERPRET_CLR_L_An_Addr(2)
INTERPRET_CLR_L_An_Addr(3)
INTERPRET_CLR_L_An_Addr(4)
INTERPRET_CLR_L_An_Addr(5)
INTERPRET_CLR_L_An_Addr(6)
INTERPRET_CLR_L_An_Addr(7)


#define INTERPRET_CLR_B_An_PreDec(dn) \
void INTERPRET_CLR_B_A##dn##_PreDec(uint32_t) \
{ \
    uint16_t sr = SR & ~SR_NZVC; \
    A##dn -= ((dn) == 7) ? 2 : 1; \
    *(uint8_t*)(uintptr_t)A##dn = 0; \
    SR = sr | SR_Z; \
    PC += 2; \
}

INTERPRET_CLR_B_An_PreDec(0)
INTERPRET_CLR_B_An_PreDec(1)
INTERPRET_CLR_B_An_PreDec(2)
INTERPRET_CLR_B_An_PreDec(3)
INTERPRET_CLR_B_An_PreDec(4)
INTERPRET_CLR_B_An_PreDec(5)
INTERPRET_CLR_B_An_PreDec(6)
INTERPRET_CLR_B_An_PreDec(7)

#define INTERPRET_CLR_W_An_PreDec(dn) \
void INTERPRET_CLR_W_A##dn##_PreDec(uint32_t) \
{ \
    uint16_t sr = SR & ~SR_NZVC; \
    A##dn -= 2; \
    *(uint16_t*)(uintptr_t)A##dn = 0; \
    SR = sr | SR_Z; \
    PC += 2; \
}

INTERPRET_CLR_W_An_PreDec(0)
INTERPRET_CLR_W_An_PreDec(1)
INTERPRET_CLR_W_An_PreDec(2)
INTERPRET_CLR_W_An_PreDec(3)
INTERPRET_CLR_W_An_PreDec(4)
INTERPRET_CLR_W_An_PreDec(5)
INTERPRET_CLR_W_An_PreDec(6)
INTERPRET_CLR_W_An_PreDec(7)

#define INTERPRET_CLR_L_An_PreDec(dn) \
void INTERPRET_CLR_L_A##dn##_PreDec(uint32_t) \
{ \
    uint16_t sr = SR & ~SR_NZVC; \
    A##dn -= 4; \
    *(uint32_t*)(uintptr_t)A##dn = 0; \
    SR = sr | SR_Z; \
    PC += 2; \
}

INTERPRET_CLR_L_An_PreDec(0)
INTERPRET_CLR_L_An_PreDec(1)
INTERPRET_CLR_L_An_PreDec(2)
INTERPRET_CLR_L_An_PreDec(3)
INTERPRET_CLR_L_An_PreDec(4)
INTERPRET_CLR_L_An_PreDec(5)
INTERPRET_CLR_L_An_PreDec(6)
INTERPRET_CLR_L_An_PreDec(7)


#define INTERPRET_CLR_B_An_PostInc(dn) \
void INTERPRET_CLR_B_A##dn##_PostInc(uint32_t) \
{ \
    uint16_t sr = SR & ~SR_NZVC; \
    *(uint8_t*)(uintptr_t)A##dn = 0; \
    A##dn += ((dn) == 7) ? 2 : 1; \
    SR = sr | SR_Z; \
    PC += 2; \
}

INTERPRET_CLR_B_An_PostInc(0)
INTERPRET_CLR_B_An_PostInc(1)
INTERPRET_CLR_B_An_PostInc(2)
INTERPRET_CLR_B_An_PostInc(3)
INTERPRET_CLR_B_An_PostInc(4)
INTERPRET_CLR_B_An_PostInc(5)
INTERPRET_CLR_B_An_PostInc(6)
INTERPRET_CLR_B_An_PostInc(7)

#define INTERPRET_CLR_W_An_PostInc(dn) \
void INTERPRET_CLR_W_A##dn##_PostInc(uint32_t) \
{ \
    uint16_t sr = SR & ~SR_NZVC; \
    *(uint16_t*)(uintptr_t)A##dn = 0; \
    A##dn += 2; \
    SR = sr | SR_Z; \
    PC += 2; \
}

INTERPRET_CLR_W_An_PostInc(0)
INTERPRET_CLR_W_An_PostInc(1)
INTERPRET_CLR_W_An_PostInc(2)
INTERPRET_CLR_W_An_PostInc(3)
INTERPRET_CLR_W_An_PostInc(4)
INTERPRET_CLR_W_An_PostInc(5)
INTERPRET_CLR_W_An_PostInc(6)
INTERPRET_CLR_W_An_PostInc(7)

#define INTERPRET_CLR_L_An_PostInc(dn) \
void INTERPRET_CLR_L_A##dn##_PostInc(uint32_t) \
{ \
    uint16_t sr = SR & ~SR_NZVC; \
    *(uint32_t*)(uintptr_t)A##dn = 0; \
    A##dn += 4; \
    SR = sr | SR_Z; \
    PC += 2; \
}

INTERPRET_CLR_L_An_PostInc(0)
INTERPRET_CLR_L_An_PostInc(1)
INTERPRET_CLR_L_An_PostInc(2)
INTERPRET_CLR_L_An_PostInc(3)
INTERPRET_CLR_L_An_PostInc(4)
INTERPRET_CLR_L_An_PostInc(5)
INTERPRET_CLR_L_An_PostInc(6)
INTERPRET_CLR_L_An_PostInc(7)

void INTERPRET_RTS(uint32_t)
{
    PC = *(uint32_t *)(uintptr_t)A7;
    A7 += 4;
}

void INTERPRET_NOP(uint32_t)
{
    PC += 2;
}

#define INTERPRET_PEA_An(src) \
void INTERPRET_PEA_A##src(uint32_t) \
{ \
    A7 -= 4; \
    PC += 2; \
    *(uint32_t*)(uintptr_t)A7 = A##src; \
}

INTERPRET_PEA_An(0);
INTERPRET_PEA_An(1);
INTERPRET_PEA_An(2);
INTERPRET_PEA_An(3);
INTERPRET_PEA_An(4);
INTERPRET_PEA_An(5);
INTERPRET_PEA_An(6);
INTERPRET_PEA_An(7);

void INTERPRET_PEA_ABS_W(uint32_t)
{
    A7 -= 4;
    *(uint32_t*)(uintptr_t)A7 = *(int16_t*)(intptr_t)(PC + 2);
    PC += 4;
}

void INTERPRET_PEA_ABS_L(uint32_t)
{
    A7 -= 4;
    *(uint32_t*)(uintptr_t)A7 = *(uint32_t*)(intptr_t)(PC + 2);
    PC += 6;
}

#define INTERPRET_JMP_An(src) \
void INTERPRET_JMP_A##src(uint32_t) \
{ \
    PC = A##src; \
}

INTERPRET_JMP_An(0);
INTERPRET_JMP_An(1);
INTERPRET_JMP_An(2);
INTERPRET_JMP_An(3);
INTERPRET_JMP_An(4);
INTERPRET_JMP_An(5);
INTERPRET_JMP_An(6);
INTERPRET_JMP_An(7);

void INTERPRET_JMP_ABS_W(uint32_t)
{
    int16_t pc = *(int16_t*)(intptr_t)(PC + 2);
    PC = pc;
}

void INTERPRET_JMP_ABS_L(uint32_t)
{
    PC = *(uint32_t*)(intptr_t)(PC + 2);
}

#define INTERPRET_JSR_An(src) \
void INTERPRET_JSR_A##src(uint32_t) \
{ \
    A7 -= 4; \
    *(uint32_t*)(intptr_t)A7 = PC + 2; \
    PC = A##src; \
}

INTERPRET_JSR_An(0);
INTERPRET_JSR_An(1);
INTERPRET_JSR_An(2);
INTERPRET_JSR_An(3);
INTERPRET_JSR_An(4);
INTERPRET_JSR_An(5);
INTERPRET_JSR_An(6);
INTERPRET_JSR_An(7);

void INTERPRET_JSR_ABS_W(uint32_t)
{
    int16_t pc = *(int16_t*)(intptr_t)(PC + 2);
    A7 -= 4;
    *(uint32_t*)(intptr_t)A7 = PC + 4;
    PC = pc;
}

void INTERPRET_JMP_d16_PC(uint32_t)
{
    int16_t pc = *(int16_t*)(intptr_t)(PC + 2);
    PC += pc + 2;
}

void INTERPRET_JSR_ABS_L(uint32_t)
{
    uint32_t pc = *(int32_t*)(intptr_t)(PC + 2);
    A7 -= 4;
    *(uint32_t*)(intptr_t)A7 = PC + 6;
    PC = pc;
}

void INTERPRET_JSR_d16_PC(uint32_t)
{
    int16_t pc = *(int16_t*)(intptr_t)(PC + 2);
    A7 -= 4;
    *(uint32_t*)(intptr_t)A7 = PC + 4;
    PC += pc + 2;
}


#define INTERPRET_LINK_W_An(reg) \
void INTERPRET_LINK_W_A##reg(uint32_t) \
{ \
    int16_t displ = *(int16_t*)(intptr_t)(PC + 2); \
    A7 -= 4; \
    *(uint32_t *)(uintptr_t)A7 = A##reg; \
    A##reg = A7; \
    A7 += displ; \
    PC += 4; \
}

INTERPRET_LINK_W_An(0);
INTERPRET_LINK_W_An(1);
INTERPRET_LINK_W_An(2);
INTERPRET_LINK_W_An(3);
INTERPRET_LINK_W_An(4);
INTERPRET_LINK_W_An(5);
INTERPRET_LINK_W_An(6);
INTERPRET_LINK_W_An(7);

#define INTERPRET_LINK_L_An(reg) \
void INTERPRET_LINK_L_A##reg(uint32_t) \
{ \
    int32_t displ = *(int32_t*)(intptr_t)(PC + 2); \
    A7 -= 4; \
    *(uint32_t *)(uintptr_t)A7 = A##reg; \
    A##reg = A7; \
    A7 += displ; \
    PC += 6; \
}

INTERPRET_LINK_L_An(0);
INTERPRET_LINK_L_An(1);
INTERPRET_LINK_L_An(2);
INTERPRET_LINK_L_An(3);
INTERPRET_LINK_L_An(4);
INTERPRET_LINK_L_An(5);
INTERPRET_LINK_L_An(6);
INTERPRET_LINK_L_An(7);


#define INTERPRET_MOVEM_L_regs_to_An_Addr(reg) \
void INTERPRET_MOVEM_L_regs_to_A##reg##_Addr(uint32_t) \
{ \
    uint32_t *base = (uint32_t *)(uintptr_t)A##reg; \
    uint16_t mask = *(uint16_t*)(uintptr_t)(PC + 2); \
    if (mask & 0x0001) *base++ = D0; \
    if (mask & 0x0002) *base++ = D1; \
    if (mask & 0x0004) *base++ = D2; \
    if (mask & 0x0008) *base++ = D3; \
    if (mask & 0x0010) *base++ = D4; \
    if (mask & 0x0020) *base++ = D5; \
    if (mask & 0x0040) *base++ = D6; \
    if (mask & 0x0080) *base++ = D7; \
    if (mask & 0x0100) *base++ = A0; \
    if (mask & 0x0200) *base++ = A1; \
    if (mask & 0x0400) *base++ = A2; \
    if (mask & 0x0800) *base++ = A3; \
    if (mask & 0x1000) *base++ = A4; \
    if (mask & 0x2000) *base++ = A5; \
    if (mask & 0x4000) *base++ = A6; \
    if (mask & 0x8000) *base++ = A7; \
    PC += 4; \
}

INTERPRET_MOVEM_L_regs_to_An_Addr(0);
INTERPRET_MOVEM_L_regs_to_An_Addr(1);
INTERPRET_MOVEM_L_regs_to_An_Addr(2);
INTERPRET_MOVEM_L_regs_to_An_Addr(3);
INTERPRET_MOVEM_L_regs_to_An_Addr(4);
INTERPRET_MOVEM_L_regs_to_An_Addr(5);
INTERPRET_MOVEM_L_regs_to_An_Addr(6);
INTERPRET_MOVEM_L_regs_to_An_Addr(7);


#define INTERPRET_MOVEM_L_regs_to_An_PreDec(reg) \
void INTERPRET_MOVEM_L_regs_to_A##reg##_PreDec(uint32_t) \
{ \
    uint32_t *base = (uint32_t *)(uintptr_t)A##reg; \
    A##reg -= 4; \
    uint16_t mask = *(uint16_t*)(uintptr_t)(PC + 2); \
    if (mask & 0x0001) *--base = A7; \
    if (mask & 0x0002) *--base = A6; \
    if (mask & 0x0004) *--base = A5; \
    if (mask & 0x0008) *--base = A4; \
    if (mask & 0x0010) *--base = A3; \
    if (mask & 0x0020) *--base = A2; \
    if (mask & 0x0040) *--base = A1; \
    if (mask & 0x0080) *--base = A0; \
    if (mask & 0x0100) *--base = D7; \
    if (mask & 0x0200) *--base = D6; \
    if (mask & 0x0400) *--base = D5; \
    if (mask & 0x0800) *--base = D4; \
    if (mask & 0x1000) *--base = D3; \
    if (mask & 0x2000) *--base = D2; \
    if (mask & 0x4000) *--base = D1; \
    if (mask & 0x8000) *--base = D0; \
    A##reg = (uint32_t)(uintptr_t)base; \
    PC += 4; \
}

INTERPRET_MOVEM_L_regs_to_An_PreDec(0);
INTERPRET_MOVEM_L_regs_to_An_PreDec(1);
INTERPRET_MOVEM_L_regs_to_An_PreDec(2);
INTERPRET_MOVEM_L_regs_to_An_PreDec(3);
INTERPRET_MOVEM_L_regs_to_An_PreDec(4);
INTERPRET_MOVEM_L_regs_to_An_PreDec(5);
INTERPRET_MOVEM_L_regs_to_An_PreDec(6);
INTERPRET_MOVEM_L_regs_to_An_PreDec(7);

static constexpr std::array<INTERPRET_Function, 4096> BuildInsnTable()
{
    std::array<INTERPRET_Function, 4096> table{};

    auto fill = [&table](int first, int last, INTERPRET_Function func) {
        for (int i = first; i <= last; ++i)
            table[i] = func;
    };

    fill(00000, 07777, INTERPRET_UNIMPLEMENTED);

    table[04100] =     INTERPRET_SWAP_D0;
    table[04101] =     INTERPRET_SWAP_D1;
    table[04102] =     INTERPRET_SWAP_D2;
    table[04103] =     INTERPRET_SWAP_D3;
    table[04104] =     INTERPRET_SWAP_D0;
    table[04105] =     INTERPRET_SWAP_D1;
    table[04106] =     INTERPRET_SWAP_D2;
    table[04107] =     INTERPRET_SWAP_D3;

    table[01000] =     INTERPRET_CLR_B_D0;
    table[01001] =     INTERPRET_CLR_B_D1;
    table[01002] =     INTERPRET_CLR_B_D2;
    table[01003] =     INTERPRET_CLR_B_D3;
    table[01004] =     INTERPRET_CLR_B_D4;
    table[01005] =     INTERPRET_CLR_B_D5;
    table[01006] =     INTERPRET_CLR_B_D6;
    table[01007] =     INTERPRET_CLR_B_D7;

    table[01100] =     INTERPRET_CLR_W_D0;
    table[01101] =     INTERPRET_CLR_W_D1;
    table[01102] =     INTERPRET_CLR_W_D2;
    table[01103] =     INTERPRET_CLR_W_D3;
    table[01104] =     INTERPRET_CLR_W_D4;
    table[01105] =     INTERPRET_CLR_W_D5;
    table[01106] =     INTERPRET_CLR_W_D6;
    table[01107] =     INTERPRET_CLR_W_D7;

    table[01200] =     INTERPRET_CLR_L_D0;
    table[01201] =     INTERPRET_CLR_L_D1;
    table[01202] =     INTERPRET_CLR_L_D2;
    table[01203] =     INTERPRET_CLR_L_D3;
    table[01204] =     INTERPRET_CLR_L_D4;
    table[01205] =     INTERPRET_CLR_L_D5;
    table[01206] =     INTERPRET_CLR_L_D6;
    table[01207] =     INTERPRET_CLR_L_D7;

    table[01020] =     INTERPRET_CLR_B_A0_Addr;
    table[01021] =     INTERPRET_CLR_B_A1_Addr;
    table[01022] =     INTERPRET_CLR_B_A2_Addr;
    table[01023] =     INTERPRET_CLR_B_A3_Addr;
    table[01024] =     INTERPRET_CLR_B_A4_Addr;
    table[01025] =     INTERPRET_CLR_B_A5_Addr;
    table[01026] =     INTERPRET_CLR_B_A6_Addr;
    table[01027] =     INTERPRET_CLR_B_A7_Addr;

    table[01120] =     INTERPRET_CLR_W_A0_Addr;
    table[01121] =     INTERPRET_CLR_W_A1_Addr;
    table[01122] =     INTERPRET_CLR_W_A2_Addr;
    table[01123] =     INTERPRET_CLR_W_A3_Addr;
    table[01124] =     INTERPRET_CLR_W_A4_Addr;
    table[01125] =     INTERPRET_CLR_W_A5_Addr;
    table[01126] =     INTERPRET_CLR_W_A6_Addr;
    table[01127] =     INTERPRET_CLR_W_A7_Addr;

    table[01220] =     INTERPRET_CLR_L_A0_Addr;
    table[01221] =     INTERPRET_CLR_L_A1_Addr;
    table[01222] =     INTERPRET_CLR_L_A2_Addr;
    table[01223] =     INTERPRET_CLR_L_A3_Addr;
    table[01224] =     INTERPRET_CLR_L_A4_Addr;
    table[01225] =     INTERPRET_CLR_L_A5_Addr;
    table[01226] =     INTERPRET_CLR_L_A6_Addr;
    table[01227] =     INTERPRET_CLR_L_A7_Addr;

    table[01030] =     INTERPRET_CLR_B_A0_PostInc;
    table[01031] =     INTERPRET_CLR_B_A1_PostInc;
    table[01032] =     INTERPRET_CLR_B_A2_PostInc;
    table[01033] =     INTERPRET_CLR_B_A3_PostInc;
    table[01034] =     INTERPRET_CLR_B_A4_PostInc;
    table[01035] =     INTERPRET_CLR_B_A5_PostInc;
    table[01036] =     INTERPRET_CLR_B_A6_PostInc;
    table[01037] =     INTERPRET_CLR_B_A7_PostInc;

    table[01130] =     INTERPRET_CLR_W_A0_PostInc;
    table[01131] =     INTERPRET_CLR_W_A1_PostInc;
    table[01132] =     INTERPRET_CLR_W_A2_PostInc;
    table[01133] =     INTERPRET_CLR_W_A3_PostInc;
    table[01134] =     INTERPRET_CLR_W_A4_PostInc;
    table[01135] =     INTERPRET_CLR_W_A5_PostInc;
    table[01136] =     INTERPRET_CLR_W_A6_PostInc;
    table[01137] =     INTERPRET_CLR_W_A7_PostInc;

    table[01230] =     INTERPRET_CLR_L_A0_PostInc;
    table[01231] =     INTERPRET_CLR_L_A1_PostInc;
    table[01232] =     INTERPRET_CLR_L_A2_PostInc;
    table[01233] =     INTERPRET_CLR_L_A3_PostInc;
    table[01234] =     INTERPRET_CLR_L_A4_PostInc;
    table[01235] =     INTERPRET_CLR_L_A5_PostInc;
    table[01236] =     INTERPRET_CLR_L_A6_PostInc;
    table[01237] =     INTERPRET_CLR_L_A7_PostInc;

    table[01040] =     INTERPRET_CLR_B_A0_PreDec;
    table[01041] =     INTERPRET_CLR_B_A1_PreDec;
    table[01042] =     INTERPRET_CLR_B_A2_PreDec;
    table[01043] =     INTERPRET_CLR_B_A3_PreDec;
    table[01044] =     INTERPRET_CLR_B_A4_PreDec;
    table[01045] =     INTERPRET_CLR_B_A5_PreDec;
    table[01046] =     INTERPRET_CLR_B_A6_PreDec;
    table[01047] =     INTERPRET_CLR_B_A7_PreDec;

    table[01140] =     INTERPRET_CLR_W_A0_PreDec;
    table[01141] =     INTERPRET_CLR_W_A1_PreDec;
    table[01142] =     INTERPRET_CLR_W_A2_PreDec;
    table[01143] =     INTERPRET_CLR_W_A3_PreDec;
    table[01144] =     INTERPRET_CLR_W_A4_PreDec;
    table[01145] =     INTERPRET_CLR_W_A5_PreDec;
    table[01146] =     INTERPRET_CLR_W_A6_PreDec;
    table[01147] =     INTERPRET_CLR_W_A7_PreDec;

    table[01240] =     INTERPRET_CLR_L_A0_PreDec;
    table[01241] =     INTERPRET_CLR_L_A1_PreDec;
    table[01242] =     INTERPRET_CLR_L_A2_PreDec;
    table[01243] =     INTERPRET_CLR_L_A3_PreDec;
    table[01244] =     INTERPRET_CLR_L_A4_PreDec;
    table[01245] =     INTERPRET_CLR_L_A5_PreDec;
    table[01246] =     INTERPRET_CLR_L_A6_PreDec;
    table[01247] =     INTERPRET_CLR_L_A7_PreDec;

    table[04010] =     INTERPRET_LINK_L_A0;
    table[04011] =     INTERPRET_LINK_L_A1;
    table[04012] =     INTERPRET_LINK_L_A2;
    table[04013] =     INTERPRET_LINK_L_A3;
    table[04014] =     INTERPRET_LINK_L_A4;
    table[04015] =     INTERPRET_LINK_L_A5;
    table[04016] =     INTERPRET_LINK_L_A6;
    table[04017] =     INTERPRET_LINK_L_A7;

    table[04120] =     INTERPRET_PEA_A0;
    table[04121] =     INTERPRET_PEA_A1;
    table[04122] =     INTERPRET_PEA_A2;
    table[04123] =     INTERPRET_PEA_A3;
    table[04124] =     INTERPRET_PEA_A4;
    table[04125] =     INTERPRET_PEA_A5;
    table[04126] =     INTERPRET_PEA_A6;
    table[04127] =     INTERPRET_PEA_A7;

    table[04170] =     INTERPRET_PEA_ABS_W;
    table[04171] =     INTERPRET_PEA_ABS_L;

    table[04320] =     INTERPRET_MOVEM_L_regs_to_A0_Addr;
    table[04321] =     INTERPRET_MOVEM_L_regs_to_A1_Addr;
    table[04322] =     INTERPRET_MOVEM_L_regs_to_A2_Addr;
    table[04323] =     INTERPRET_MOVEM_L_regs_to_A3_Addr;
    table[04324] =     INTERPRET_MOVEM_L_regs_to_A4_Addr;
    table[04325] =     INTERPRET_MOVEM_L_regs_to_A5_Addr;
    table[04326] =     INTERPRET_MOVEM_L_regs_to_A6_Addr;
    table[04327] =     INTERPRET_MOVEM_L_regs_to_A7_Addr;

    table[04340] =     INTERPRET_MOVEM_L_regs_to_A0_PreDec;
    table[04341] =     INTERPRET_MOVEM_L_regs_to_A1_PreDec;
    table[04342] =     INTERPRET_MOVEM_L_regs_to_A2_PreDec;
    table[04343] =     INTERPRET_MOVEM_L_regs_to_A3_PreDec;
    table[04344] =     INTERPRET_MOVEM_L_regs_to_A4_PreDec;
    table[04345] =     INTERPRET_MOVEM_L_regs_to_A5_PreDec;
    table[04346] =     INTERPRET_MOVEM_L_regs_to_A6_PreDec;
    table[04347] =     INTERPRET_MOVEM_L_regs_to_A7_PreDec;

    table[07120] =     INTERPRET_LINK_W_A0;
    table[07121] =     INTERPRET_LINK_W_A1;
    table[07122] =     INTERPRET_LINK_W_A2;
    table[07123] =     INTERPRET_LINK_W_A3;
    table[07124] =     INTERPRET_LINK_W_A4;
    table[07125] =     INTERPRET_LINK_W_A5;
    table[07126] =     INTERPRET_LINK_W_A6;
    table[07127] =     INTERPRET_LINK_W_A7;

    table[07161] =     INTERPRET_NOP;
    table[07165] =     INTERPRET_RTS;

    table[07220] =     INTERPRET_JSR_A0;
    table[07221] =     INTERPRET_JSR_A1;
    table[07222] =     INTERPRET_JSR_A0;
    table[07223] =     INTERPRET_JSR_A1;
    table[07224] =     INTERPRET_JSR_A0;
    table[07225] =     INTERPRET_JSR_A1;
    table[07226] =     INTERPRET_JSR_A0;
    table[07227] =     INTERPRET_JSR_A1;

    table[07270] =     INTERPRET_JSR_ABS_W;
    table[07271] =     INTERPRET_JSR_ABS_L;
    table[07272] =     INTERPRET_JSR_d16_PC;

    table[07220] =     INTERPRET_JMP_A0;
    table[07221] =     INTERPRET_JMP_A1;
    table[07222] =     INTERPRET_JMP_A0;
    table[07223] =     INTERPRET_JMP_A1;
    table[07224] =     INTERPRET_JMP_A0;
    table[07225] =     INTERPRET_JMP_A1;
    table[07226] =     INTERPRET_JMP_A0;
    table[07227] =     INTERPRET_JMP_A1;

    table[07370] =     INTERPRET_JMP_ABS_W;
    table[07371] =     INTERPRET_JMP_ABS_L;
    table[07272] =     INTERPRET_JMP_d16_PC;
    
    #if 0
    [00300 ... 00307] = { EMIT_MOVEfromSR, NULL, SR_ALL, 0, 1, 0, 2 },
    [00320 ... 00347] = { EMIT_MOVEfromSR, NULL, SR_ALL, 0, 1, 0, 2 },
    [00350 ... 00371] = { EMIT_MOVEfromSR, NULL, SR_ALL, 0, 1, 1, 2 },

    [01300 ... 01307] = { EMIT_MOVEfromCCR, NULL, SR_CCR, 0, 1, 0, 2 },
    [01320 ... 01347] = { EMIT_MOVEfromCCR, NULL, SR_CCR, 0, 1, 0, 2 },
    [01350 ... 01371] = { EMIT_MOVEfromCCR, NULL, SR_CCR, 0, 1, 1, 2 },

    [03300 ... 03307] = { EMIT_MOVEtoSR, NULL, SR_S, SR_ALL, 1, 0, 2 },
    [03320 ... 03347] = { EMIT_MOVEtoSR, NULL, SR_S, SR_ALL, 1, 0, 2 },
    [03350 ... 03374] = { EMIT_MOVEtoSR, NULL, SR_S, SR_ALL, 1, 1, 2 },

    [02300 ... 02307] = { EMIT_MOVEtoCCR, NULL, 0, SR_CCR, 1, 0, 2 },
    [02320 ... 02347] = { EMIT_MOVEtoCCR, NULL, 0, SR_CCR, 1, 0, 2 },
    [02350 ... 02374] = { EMIT_MOVEtoCCR, NULL, 0, SR_CCR, 1, 1, 2 },

    [04200 ... 04207] = { EMIT_EXT, NULL, 0, SR_NZVC, 1, 0, 0 },
    [04300 ... 04307] = { EMIT_EXT, NULL, 0, SR_NZVC, 1, 0, 0 },
    [04700 ... 04707] = { EMIT_EXT, NULL, 0, SR_NZVC, 1, 0, 0 },

    [04010 ... 04017] = { EMIT_LINK32, NULL, 0, 0, 3, 0, 0 },
    [07120 ... 07127] = { EMIT_LINK16, NULL, 0, 0, 2, 0, 0 },

    [0xafc]           = { EMIT_ILLEGAL, NULL, SR_CCR, 0, 1, 0, 0 },
    [0xe40 ... 0xe4f] = { EMIT_TRAP, NULL, SR_CCR, 0, 1, 0, 0 },
    [07130 ... 07137] = { EMIT_UNLK, NULL, 0, 0, 1, 0, 0 },
    [0xe70]           = { EMIT_RESET, NULL, SR_S, 0, 1, 0, 0 },
    
    [0xe72]           = { EMIT_STOP, NULL, SR_S, SR_ALL, 2, 0, 0 },
    [0xe73]           = { EMIT_RTE, NULL, SR_S, SR_ALL, 1, 0, 0 },
    [0xe74]           = { EMIT_RTD, NULL, 0, 0, 2, 0, 0 },
    
    [0xe76]           = { EMIT_TRAPV, NULL, SR_CCR, 0, 1, 0, 0 },
    [0xe77]           = { EMIT_RTR, NULL, 0, SR_CCR, 1, 0, 0 },
    [0xe7a ... 0xe7b] = { EMIT_MOVEC, NULL, SR_S, 0, 2, 0, 4 },
    [0xe60 ... 0xe6f] = { EMIT_MOVEUSP, NULL, SR_S, 0, 1, 0, 4 },
    [04110 ... 04117] = { EMIT_BKPT, NULL, SR_ALL, 0, 1, 0, 0 },      // BKPT

    [07320 ... 07327] = { EMIT_JMP, NULL, 0, 0, 1, 0, 0 },
    [07350 ... 07373] = { EMIT_JMP, NULL, 0, 0, 1, 1, 0 },

    [07220 ... 07227] = { EMIT_JSR, NULL, 0, 0, 1, 0, 0 },
    [07250 ... 07273] = { EMIT_JSR, NULL, 0, 0, 1, 1, 0 },

    [00000 ... 00007] = { EMIT_NEGX, NULL, SR_XZ, SR_CCR, 1, 0, 1 },
    [00100 ... 00107] = { EMIT_NEGX, NULL, SR_XZ, SR_CCR, 1, 0, 2 },
    [00200 ... 00207] = { EMIT_NEGX, NULL, SR_XZ, SR_CCR, 1, 0, 4 },

    [00020 ... 00047] = { EMIT_NEGX, NULL, SR_XZ, SR_CCR, 1, 0, 1 },
    [00120 ... 00147] = { EMIT_NEGX, NULL, SR_XZ, SR_CCR, 1, 0, 2 },
    [00220 ... 00247] = { EMIT_NEGX, NULL, SR_XZ, SR_CCR, 1, 0, 4 },
    
    [00050 ... 00071] = { EMIT_NEGX, NULL, SR_XZ, SR_CCR, 1, 1, 1 },
    [00150 ... 00171] = { EMIT_NEGX, NULL, SR_XZ, SR_CCR, 1, 1, 2 },
    [00250 ... 00271] = { EMIT_NEGX, NULL, SR_XZ, SR_CCR, 1, 1, 4 },


    [01020 ... 01047] = { EMIT_CLR, NULL, 0, SR_NZVC, 1, 0, 1 },
    [01120 ... 01147] = { EMIT_CLR, NULL, 0, SR_NZVC, 1, 0, 2 },
    [01220 ... 01247] = { EMIT_CLR, NULL, 0, SR_NZVC, 1, 0, 4 },

    [01050 ... 01071] = { EMIT_CLR, NULL, 0, SR_NZVC, 1, 1, 1 },
    [01150 ... 01171] = { EMIT_CLR, NULL, 0, SR_NZVC, 1, 1, 2 },
    [01250 ... 01271] = { EMIT_CLR, NULL, 0, SR_NZVC, 1, 1, 4 },

    [02000 ... 02007] = { EMIT_NEG, NULL, 0, SR_CCR, 1, 0, 1 },
    [02100 ... 02107] = { EMIT_NEG, NULL, 0, SR_CCR, 1, 0, 2 },
    [02200 ... 02207] = { EMIT_NEG, NULL, 0, SR_CCR, 1, 0, 4 },

    [02020 ... 02047] = { EMIT_NEG, NULL, 0, SR_CCR, 1, 0, 1 },
    [02120 ... 02147] = { EMIT_NEG, NULL, 0, SR_CCR, 1, 0, 2 },
    [02220 ... 02247] = { EMIT_NEG, NULL, 0, SR_CCR, 1, 0, 4 },

    [02050 ... 02071] = { EMIT_NEG, NULL, 0, SR_CCR, 1, 1, 1 },
    [02150 ... 02171] = { EMIT_NEG, NULL, 0, SR_CCR, 1, 1, 2 },
    [02250 ... 02271] = { EMIT_NEG, NULL, 0, SR_CCR, 1, 1, 4 },

    [03000 ... 03007] = { EMIT_NOT, NULL, 0, SR_NZVC, 1, 0, 1 },
    [03100 ... 03107] = { EMIT_NOT, NULL, 0, SR_NZVC, 1, 0, 2 },
    [03200 ... 03207] = { EMIT_NOT, NULL, 0, SR_NZVC, 1, 0, 4 },

    [03020 ... 03047] = { EMIT_NOT, NULL, 0, SR_NZVC, 1, 0, 1 },
    [03120 ... 03147] = { EMIT_NOT, NULL, 0, SR_NZVC, 1, 0, 2 },
    [03220 ... 03247] = { EMIT_NOT, NULL, 0, SR_NZVC, 1, 0, 4 },

    [03050 ... 03071] = { EMIT_NOT, NULL, 0, SR_NZVC, 1, 1, 1 },
    [03150 ... 03171] = { EMIT_NOT, NULL, 0, SR_NZVC, 1, 1, 2 },
    [03250 ... 03271] = { EMIT_NOT, NULL, 0, SR_NZVC, 1, 1, 4 },

    [05000 ... 05007] = { EMIT_TST, NULL, 0, SR_NZVC, 1, 0, 1 },
    [05020 ... 05047] = { EMIT_TST, NULL, 0, SR_NZVC, 1, 0, 1 },
    [05050 ... 05074] = { EMIT_TST, NULL, 0, SR_NZVC, 1, 1, 1 },
    
    [05100 ... 05147] = { EMIT_TST, NULL, 0, SR_NZVC, 1, 0, 2 },
    [05150 ... 05174] = { EMIT_TST, NULL, 0, SR_NZVC, 1, 1, 2 },
    
    [05200 ... 05247] = { EMIT_TST, NULL, 0, SR_NZVC, 1, 0, 4 },
    [05250 ... 05274] = { EMIT_TST, NULL, 0, SR_NZVC, 1, 1, 4 },

    [04000 ... 04007] = { EMIT_NBCD, NULL, SR_XZ, SR_XZC, 1, 0, 1 },
    [04020 ... 04047] = { EMIT_NBCD, NULL, SR_XZ, SR_XZC, 1, 0, 1 },
    [04050 ... 04071] = { EMIT_NBCD, NULL, SR_XZ, SR_XZC, 1, 1, 1 },

    [04120 ... 04127] = { EMIT_PEA, NULL, 0, 0, 1, 0, 4 },
    [04150 ... 04173] = { EMIT_PEA, NULL, 0, 0, 1, 1, 4 },

    [05300 ... 05307] = { EMIT_TAS, NULL, 0, SR_NZVC, 1, 0, 1 },
    [05320 ... 05347] = { EMIT_TAS, NULL, 0, SR_NZVC, 1, 0, 1 },
    [05350 ... 05371] = { EMIT_TAS, NULL, 0, SR_NZVC, 1, 1, 1 },

    [06000 ... 06007] = { EMIT_MUL_DIV, NULL, 0, SR_NZVC, 2, 0, 4 },
    [06020 ... 06047] = { EMIT_MUL_DIV, NULL, 0, SR_NZVC, 2, 0, 4 },
    [06050 ... 06074] = { EMIT_MUL_DIV, NULL, 0, SR_NZVC, 2, 1, 4 },
    [06100 ... 06107] = { EMIT_MUL_DIV, NULL, 0, SR_NZVC, 2, 0, 4 },
    [06120 ... 06147] = { EMIT_MUL_DIV, NULL, 0, SR_NZVC, 2, 0, 4 },
    [06150 ... 06174] = { EMIT_MUL_DIV, NULL, 0, SR_NZVC, 2, 1, 4 },

    [04220 ... 04227] = { EMIT_MOVEM, NULL, 0, 0, 2, 0, 2 },
    [04320 ... 04327] = { EMIT_MOVEM, NULL, 0, 0, 2, 0, 4 },
    [04240 ... 04247] = { EMIT_MOVEM, NULL, 0, 0, 2, 0, 2 },
    [04340 ... 04347] = { EMIT_MOVEM, NULL, 0, 0, 2, 0, 4 },
    [04250 ... 04271] = { EMIT_MOVEM, NULL, 0, 0, 2, 1, 2 },
    [04350 ... 04371] = { EMIT_MOVEM, NULL, 0, 0, 2, 1, 4 },

    [06220 ... 06237] = { EMIT_MOVEM, NULL, 0, 0, 2, 0, 2 },
    [06320 ... 06337] = { EMIT_MOVEM, NULL, 0, 0, 2, 0, 4 },
    [06250 ... 06273] = { EMIT_MOVEM, NULL, 0, 0, 2, 1, 2 },
    [06350 ... 06373] = { EMIT_MOVEM, NULL, 0, 0, 2, 1, 4 },

    [00720 ... 00727] = { EMIT_LEA, NULL, 0, 0, 1, 0, 4 },
    [00750 ... 00773] = { EMIT_LEA, NULL, 0, 0, 1, 1, 4 },
    [01720 ... 01727] = { EMIT_LEA, NULL, 0, 0, 1, 0, 4 },
    [01750 ... 01773] = { EMIT_LEA, NULL, 0, 0, 1, 1, 4 },
    [02720 ... 02727] = { EMIT_LEA, NULL, 0, 0, 1, 0, 4 },
    [02750 ... 02773] = { EMIT_LEA, NULL, 0, 0, 1, 1, 4 },
    [03720 ... 03727] = { EMIT_LEA, NULL, 0, 0, 1, 0, 4 },
    [03750 ... 03773] = { EMIT_LEA, NULL, 0, 0, 1, 1, 4 },
    [04720 ... 04727] = { EMIT_LEA, NULL, 0, 0, 1, 0, 4 },
    [04750 ... 04773] = { EMIT_LEA, NULL, 0, 0, 1, 1, 4 },
    [05720 ... 05727] = { EMIT_LEA, NULL, 0, 0, 1, 0, 4 },
    [05750 ... 05773] = { EMIT_LEA, NULL, 0, 0, 1, 1, 4 },
    [06720 ... 06727] = { EMIT_LEA, NULL, 0, 0, 1, 0, 4 },
    [06750 ... 06773] = { EMIT_LEA, NULL, 0, 0, 1, 1, 4 },
    [07720 ... 07727] = { EMIT_LEA, NULL, 0, 0, 1, 0, 4 },
    [07750 ... 07773] = { EMIT_LEA, NULL, 0, 0, 1, 1, 4 },

    [00600 ... 00607] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [00620 ... 00647] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [00650 ... 00674] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 1, 2 },
    [00400 ... 00407] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [00420 ... 00447] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [00450 ... 00474] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 1, 4 },

    [01600 ... 01607] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [01620 ... 01647] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [01650 ... 01674] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 1, 2 },
    [01400 ... 01407] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [01420 ... 01447] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [01450 ... 01474] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 1, 4 },

    [02600 ... 02607] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [02620 ... 02647] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [02650 ... 02674] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 1, 2 },
    [02400 ... 02407] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [02420 ... 02447] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [02450 ... 02474] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 1, 4 },

    [03600 ... 03607] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [03620 ... 03647] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [03650 ... 03674] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 1, 2 },
    [03400 ... 03407] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [03420 ... 03447] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [03450 ... 03474] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 1, 4 },

    [04600 ... 04607] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [04620 ... 04647] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [04650 ... 04674] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 1, 2 },
    [04400 ... 04407] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [04420 ... 04447] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [04450 ... 04474] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 1, 4 },

    [05600 ... 05607] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [05620 ... 05647] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [05650 ... 05674] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 1, 2 },
    [05400 ... 05407] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [05420 ... 05447] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [05450 ... 05474] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 1, 4 },

    [06600 ... 06607] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [06620 ... 06647] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [06650 ... 06674] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 1, 2 },
    [06400 ... 06407] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [06420 ... 06447] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [06450 ... 06474] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 1, 4 },

    [07600 ... 07607] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [07620 ... 07647] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 2 },
    [07650 ... 07674] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 1, 2 },
    [07400 ... 07407] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [07420 ... 07447] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 0, 4 },
    [07450 ... 07474] = { EMIT_CHK, NULL, SR_CCR, SR_NZVC, 1, 1, 4 },
    #endif

    return table;
}

static constexpr auto InsnTable = BuildInsnTable();

__attribute__((optimize("no-optimize-sibling-calls")))
void INTERPRET_line4(uint32_t opcode)
{
    InsnTable[opcode & 0xfff](opcode);
}
