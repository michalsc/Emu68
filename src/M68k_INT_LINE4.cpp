#include <cstdint>
#include <arm_neon.h>
#include <array>

#include <emu68/interpreter/Line4.hpp>

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

static inline struct M68KState *getCTX()
{
    struct M68KState *ctx;
    __asm__ volatile("mov %0, " CTX_POINTER_ASM:"=r"(ctx));
    return ctx;
}

#define INTERPRET_SWAP_Dn(dn) \
void INTERPRET_SWAP_D##dn(uint16_t) \
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
void INTERPRET_CLR_B_D##dn(uint16_t) \
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
void INTERPRET_CLR_W_D##dn(uint16_t) \
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
void INTERPRET_CLR_L_D##dn(uint16_t) \
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

#if 0

uint32_t INTERPRET_CLR_W_REG(uint16_t opcode, struct M68KState *state)
{
    uint8_t dn = opcode & 7;

    state->D[dn].u16[1] = 0;
    state->SR &= ~(SR_N | SR_Z | SR_V | SR_C);
    state->SR |= SR_Z;
    state->PC += 2;
    
    return state->PC;
}

uint32_t INTERPRET_CLR_L_REG(uint16_t opcode, struct M68KState *state)
{
    uint8_t dn = opcode & 7;

    state->D[dn].u32 = 0;
    state->SR &= ~(SR_N | SR_Z | SR_V | SR_C);
    state->SR |= SR_Z;
    state->PC += 2;
    
    return state->PC;
}
#endif

void INTERPRET_RTS(uint16_t)
{
    PC = *(uint32_t *)(uintptr_t)A7;
    A7 += 4;
}

void INTERPRET_NOP(uint16_t)
{
    PC += 2;
}

void INTERPRET_Exception_F0(uint32_t exception)
{
    /* Get the SR register as it is now */
    uint32_t origSR = SR;

    /* If we were in user mode, switch stack and reserve space for exception frame */
    if (likely(origSR & SR_S) == 0) {
        USP = A7;
        if (unlikely(origSR & SR_M)) {
            A7 = MSP - 8;
        }
        else {
            A7 = ISP - 8;
        }
    } else {
        A7 -= 8;
    }

    /* Push the exception frame */
    void *sp = (void *)(uintptr_t)A7;

    /* Invert V and C flags */
    uint32_t tmp = origSR;
    uint32_t tmp2;

    asm volatile("rbit %0, %1":"=r"(tmp2):"r"(tmp));
    tmp = (tmp & ~3) | ((tmp2 >> 30) & 3);

    /* Prepare frame */
    *(uint16_t *)(uintptr_t)sp = tmp;
    *(uint32_t *)((uintptr_t)sp + 2) = PC;
    *(uint16_t *)((uintptr_t)sp + 6) = exception;

    /* Set SR to supervisor and clear trace flags */
    origSR |= SR_S;
    origSR &= ~(SR_T0 | SR_T1);

    /* Set the new SR */
    SR = origSR;

    /* Get the vector address */
    uint32_t vbr = getCTX()->VBR + (exception & 0x0fff);
    PC = *(uint32_t *)(uintptr_t)vbr;
}

void INTERPRET_UNIMPLEMENTED(uint16_t opcode)
{
    kprintf("[INT] opcode %04x at %08x not implemented\n", opcode, PC);

    INTERPRET_Exception_F0(VECTOR_ILLEGAL_INSTRUCTION);
}

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

    table[07161] =     INTERPRET_NOP;
    table[07165] =     INTERPRET_RTS;
    
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
