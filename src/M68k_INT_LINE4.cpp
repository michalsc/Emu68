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

register uint64_t (*ARMCode)() __asm__("x12");

extern "C" {

    extern int disasm;
    extern int debug;
    extern uint32_t debug_range_min;
    extern uint32_t debug_range_max;
    void M68K_LoadContext(struct M68KState *ctx);
    void M68K_SaveContext(struct M68KState *ctx);

}

namespace Emu68::M68k::Interpreter {



template<uint8_t Mode, uint8_t Reg>
void MOVE_to_CCR(uint32_t)
{
    PC += 2;
    WORD val = LoadFromEA<Mode, Reg, WORD>() & SR_CCR;
    SR = (SR & 0xff00) | (val & 0x00ff);
}

template<uint8_t Mode, uint8_t Reg>
void MOVE_to_SR(uint32_t)
{
    uint16_t sr = SR;
    uint16_t changed = sr;

    /* Modifying SR requires supervisor rights */
    if (sr & SR_S) {
        PC += 2;
        WORD val = LoadFromEA<Mode, Reg, WORD>() & SR_ALL;

        sr = val;
        changed ^= sr;

        HandleChangedSR(sr, changed);
        
        SR = sr;
    } else {
        Exception_F0(VECTOR_PRIVILEGE_VIOLATION);
    }
}

template<uint8_t Dn> requires (Dn < 8)
void SWAP(uint32_t)
{
    uint32_t sr = SR & ~SR_CCR;
    uint32_t val = getD<Dn, uint32_t>();

    if (unlikely(val == 0)) {
        sr |= SR_Z;
    } else {
        val = (val >> 16) | (val << 16);
        if (val & 0x80000000) {
            sr |= SR_N;
        }
        setD<Dn, uint32_t>(val);
    }
    SR = sr;
    PC += 2;
}

template<uint8_t Mode, uint8_t Reg, class Type>
void CLR(uint32_t)
{
    uint16_t sr = SR & ~SR_NZVC;
    SR = sr | SR_Z;
    PC += 2;

    StoreToEA<Mode, Reg, Type>(0);
}

template<uint8_t Mode, uint8_t Reg, class Type>
void TST(uint32_t)
{
    uint16_t sr = SR & ~SR_NZVC;
    PC += 2;
    Type val = LoadFromEA<Mode, Reg, Type>();
    if (val == 0) {
        sr |= SR_Z;
    } else if (val < 0) {
        sr |= SR_N;
    }
    SR = sr;
}

template<uint8_t Mode, uint8_t Reg, class Type>
void NOT(uint32_t)
{
    PC += 2;
    ReadModifyWriteEA<Mode, Reg, Type>([&](Type v) -> Type {
        uint32_t sr = SR & ~SR_NZVC;
        
        v = ~v;
        
        if (v == 0) {
            sr |= SR_Z;
        } else if (v < 0) {
            sr |= SR_N;
        }
        SR = sr;

        return v;
    });
}

template<uint8_t Mode, uint8_t Reg, class Type>
void NEG(uint32_t)
{
    PC += 2;
    ReadModifyWriteEA<Mode, Reg, Type>([&](Type v) -> Type {
        auto [result, ccr] = Arith_WithFlags<Type, true>(0, v);
        SR = (SR & ~(SR_X | SR_NZVC)) | ccr | ((ccr & SR_Calt) ? SR_X : 0);
        return result;
    });
}

static inline struct M68KState *getCTX()
{
    struct M68KState *ctx;
    __asm__ volatile("mov %0, " CTX_POINTER_ASM:"=r"(ctx));
    return ctx;
}

void TRAP(uint32_t opcode)
{
    Exception_F0(VECTOR_INT_TRAP(opcode & 15));
}

void RTS(uint32_t)
{
    PC = *(uint32_t *)(uintptr_t)A7;
    A7 += 4;
}

void RTR(uint32_t)
{
    SR = (SR & ~SR_CCR) | (*(uint16_t*)(uintptr_t)A7 & SR_CCR);
    PC = *(uint32_t *)(uintptr_t)(A7 + 2);
    A7 += 6;
}

void RTD(uint32_t)
{
    int32_t displacement = *(int16_t*)(uintptr_t)(PC + 2);
    PC = *(uint32_t *)(uintptr_t)A7;
    A7 += 4 + displacement;
}

void NOP(uint32_t)
{
    PC += 2;
}

template<uint8_t Mode, uint8_t Reg, uint8_t An>
void LEA(uint32_t)
{
    PC += 2;
    setA<An, uint32_t>(GetEA<Mode, Reg, uint32_t>());
}

template<uint8_t Mode, uint8_t Reg>
void PEA(uint32_t)
{
    PC += 2;
    A7 -= 4;
    *(uint32_t*)(uintptr_t)A7 = GetEA<Mode, Reg, uint32_t>();
}

template<uint8_t Mode, uint8_t Reg>
void JMP(uint32_t)
{
    PC += 2;
    PC = GetEA<Mode, Reg, uint32_t>();
}

template<uint8_t Mode, uint8_t Reg>
void JSR(uint32_t)
{
    uint32_t new_pc;
    PC += 2;
    A7 -= 4;
    new_pc = GetEA<Mode, Reg, uint32_t>();
    *(uint32_t*)(intptr_t)A7 = PC; 
    PC = new_pc;
}

template<uint8_t An>
void LINK_W(uint32_t)
{
    int16_t displ = *(int16_t*)(intptr_t)(PC + 2);
    A7 -= 4;
    *(uint32_t *)(uintptr_t)A7 = getA<An, uint32_t>();
    setA<An, uint32_t>(A7);
    A7 += displ;
    PC += 4;
}

template<uint8_t An>
void LINK_L(uint32_t)
{
    int32_t displ = *(int32_t*)(intptr_t)(PC + 2);
    A7 -= 4;
    *(uint32_t *)(uintptr_t)A7 = getA<An, uint32_t>();
    setA<An, uint32_t>(A7);
    A7 += displ;
    PC += 6;
}

template<uint8_t An>
void UNLK(uint32_t)
{
    A7 = getA<An, uint32_t>();
    setA<An, uint32_t>(*(uint32_t*)(uintptr_t)A7);
    A7 += 4;
    PC += 2;
}

template<uint8_t Dn>
void EXT_B_to_W(uint32_t)
{
    uint32_t sr = SR & ~SR_NZVC;
    PC += 2;
    WORD val = getD<Dn, BYTE>();
    setD<Dn, WORD>(val);

    if (val == 0) {
        sr |= SR_Z;
    } else if (val < 0) {
        sr |= SR_N;
    }

    SR = sr;
}

template<uint8_t Dn>
void EXT_W_to_L(uint32_t)
{
    uint32_t sr = SR & ~SR_NZVC;
    PC += 2;
    LONG val = getD<Dn, WORD>();
    setD<Dn, LONG>(val);

    if (val == 0) {
        sr |= SR_Z;
    } else if (val < 0) {
        sr |= SR_N;
    }

    SR = sr;
}

template<uint8_t Dn>
void EXT_B_to_L(uint32_t)
{
    uint32_t sr = SR & ~SR_NZVC;
    PC += 2;
    LONG val = getD<Dn, BYTE>();
    setD<Dn, LONG>(val);

    if (val == 0) {
        sr |= SR_Z;
    } else if (val < 0) {
        sr |= SR_N;
    }

    SR = sr;
}

void MOVEC(uint32_t opcode)
{
    /* MOVEC requires supervisor rights */
    if (SR & SR_S)
    {
        struct M68KState *ctx = getCTX();
        uint16_t opcode2 = *(uint16_t*)(uintptr_t)(PC + 2);
        uint16_t cr = opcode2 & 0x0fff;
        uint8_t reg = (opcode2 >> 12) & 7;
        
        if (opcode & 1)
        {
            // Register to Control Register
            uint32_t dn = opcode2 & 0x8000 ? getAn(reg) : getDn(reg);

            switch(cr)
            {
                case 0x000: ctx->SFC = dn & 7;                      break;
                case 0x001: ctx->DFC = dn & 7;                      break;
                case 0x002: CACR = dn & 0x80008000; ARMCode = 0;    break;
                case 0x003: ctx->TCR = (dn & 0xc000);               break;
                case 0x004: ctx->ITT0 = dn & 0x1c9b;                break;
                case 0x005: ctx->ITT1 = dn & 0x1c9b;                break;
                case 0x006: ctx->DTT0 = dn & 0x1c9b;                break;
                case 0x007: ctx->DTT1 = dn & 0x1c9b;                break;
                case 0x800: USP = dn;                               break;
                case 0x801: ctx->VBR = dn;                          break;
                case 0x803: if (SR & SR_M) A7 = dn; else MSP = dn;  break;
                case 0x804: if (SR & SR_M) ISP = dn; else A7 = dn;  break;
                case 0x805: ctx->MMUSR = dn;                        break;
                case 0x806: ctx->URP = dn & 0xfffffe00;             break;
                case 0x807: ctx->SRP = dn & 0xfffffe00;             break;
                case 0x0ea: ctx->JIT_SOFTFLUSH_THRESH = dn;         break;
                case 0x0eb: ctx->JIT_CONTROL = dn;                  break;
                case 0x0ed: debug = dn & 3; disasm = (dn >> 2) & 1; break;
                case 0x0ee: debug_range_min = dn;                   break;
                case 0x0ef: debug_range_max = dn;                   break;
                case 0x1e0: ctx->JIT_CONTROL2 = dn & 0x1fffffff;
                            if (dn & 0x20000000) ctx->INTF.ARM = 0;
                            if (dn & 0x40000000) ctx->INTF.PPC = 0;
                            if (dn & 0x80000000) {
                                *ctx->PPC_EE_FLAG = 255;
                                asm volatile("sev":::"memory");
                            }                                       break;
                default:    Exception_F0(VECTOR_PRIVILEGE_VIOLATION); return;
            }

            /* Do not allow enabling MMU for now */
            ctx->TCR &= ~0x8000;
        }
        else
        {
            // Control Register to Register
            uint64_t val = 0;

            switch (cr) {
                case 0x000: val = ctx->SFC;                                 break;
                case 0x001: val = ctx->DFC;                                 break;
                case 0x002: val = CACR;                                     break;
                case 0x800: val = USP;                                      break;
                case 0x801: val = ctx->VBR;                                 break;
                case 0x803: val = (SR & SR_M) ? A7 : MSP;                   break;
                case 0x804: val = (SR & SR_M) ? ISP : A7;                   break;
                case 0x0e0: asm volatile("mrs %0, CNTFRQ_EL0":"=r"(val));   break;
                case 0x0e1: asm volatile("mrs %0, CNTPCT_EL0":"=r"(val));   break;
                case 0x0e2: asm volatile("mrs %0, CNTPCT_EL0":"=r"(val)); 
                            val >>= 32;                                     break;
                case 0x0e3: asm volatile("mov %w0, v22.s[0]":"=r"(val));    break;
                case 0x0e4: asm volatile("mov %w0, v22.s[1]":"=r"(val));    break;
                case 0x0e5: asm volatile("mrs %0, PMCCNTR_EL0":"=r"(val));  break;
                case 0x0e6: asm volatile("mrs %0, PMCCNTR_EL0":"=r"(val));
                            val >>= 32;                                     break;
                case 0x0e7: val = ctx->JIT_CACHE_TOTAL;                     break;
                case 0x0e8: val = ctx->JIT_CACHE_FREE;                      break;
                case 0x0e9: val = ctx->JIT_UNIT_COUNT;                      break;
                case 0x0ea: val = ctx->JIT_SOFTFLUSH_THRESH;                break;
                case 0x0eb: val = ctx->JIT_CONTROL;                         break;
                case 0x0ec: val = ctx->JIT_CACHE_MISS;                      break;
                case 0x0ed: val = (debug & 3) | disasm ? 4 : 0;             break;
                case 0x0ee: val = debug_range_min;                          break;
                case 0x0ef: val = debug_range_max;                          break;
                case 0x1e0: val = ctx->JIT_CONTROL2;
                            val |= ctx->INTF.PPC ? 0x40000000 : 0;
                            val |= ctx->INTF.ARM ? 0x20000000 : 0;          break;
                case 0x003: val = ctx->TCR;                                 break;
                case 0x004: val = ctx->ITT0;                                break;
                case 0x005: val = ctx->ITT1;                                break;
                case 0x006: val = ctx->DTT0;                                break;
                case 0x007: val = ctx->DTT1;                                break;
                case 0x805: val = ctx->MMUSR;                               break;
                case 0x806: val = ctx->URP;                                 break;
                case 0x807: val = ctx->SRP;                                 break;
                default:    Exception_F0(VECTOR_PRIVILEGE_VIOLATION);       return;
            }

            if (opcode2 & 0x8000) {
                setAn(reg, val);
            } else {
                setDn(reg, val);
            }
        }
        PC += 4;
    }
    else
    {
        Exception_F0(VECTOR_PRIVILEGE_VIOLATION);
    }
}


template<uint8_t Mode, uint8_t Reg>
void DIVU_L(uint32_t)
{
    uint32_t sr = SR & ~SR_NZVC;
    uint16_t opcode2 = *(uint16_t *)(uintptr_t)(PC + 2);
    uint32_t orig_PC = PC;

    PC += 4;

    uint32_t src = LoadFromEA<Mode, Reg, uint32_t>();

    if (src == 0) {
        Exception_F2(VECTOR_DIVIDE_BY_ZERO, orig_PC);
    }
    else {
        uint32_t dq = getDn((opcode2 >> 12) & 7);

        /* 64 / 32 -> 32 divide */
        if (opcode2 & (1 << 10)) {
            uint64_t value = getDn(opcode2 & 7);
            value = (value << 32) | dq;

            /* If reminder register is the same as destination, skip reminder */
            if ((opcode2 & 7) == ((opcode2 >> 12) & 7))
            {
                value = value / src;
                setDn((opcode2 >> 12) & 7, value);
            }
            else
            {
                uint64_t rem;
                rem = value % src;
                value = value / src;

                /* Overflow! */
                if (value & 0xffffffff00000000ULL) {
                    SR = (SR & ~SR_Calt) | SR_Valt;
                    return;
                }

                setDn(opcode2 & 7, rem);
                setDn((opcode2 >> 12) & 7, value);

                if ((int32_t)value == 0) {
                    sr |= SR_Z;
                } else if ((int32_t)value < 0) {
                    sr |= SR_N;
                }

                SR = sr;
            }
        }
        else 
        /* 32 / 32 -> 32 divide */
        {
            /* If reminder register is the same as destination, skip reminder */
            if ((opcode2 & 7) == ((opcode2 >> 12) & 7))
            {
                dq = dq / src;
                setDn((opcode2 >> 12) & 7, dq);
            }
            else
            {
                uint32_t rem;
                rem = dq % src;
                dq = dq / src;

                setDn(opcode2 & 7, rem);
                setDn((opcode2 >> 12) & 7, dq);
            }

            if ((int32_t)dq == 0) {
                sr |= SR_Z;
            } else if ((int32_t)dq < 0) {
                sr |= SR_N;
            }

            SR = sr;
        }
    }
}

template<uint8_t Mode, uint8_t Reg>
void MULU_L(uint32_t)
{
    uint32_t sr = SR & ~SR_NZVC;
    uint16_t opcode2 = *(uint16_t *)(uintptr_t)(PC + 2);

    PC += 4;

    /* Signed (bit 11 set) or unsigned (bit 11 clear) */
    if (opcode2 & (1 << 11)) {
        int32_t src = LoadFromEA<Mode, Reg, int32_t>();
        int32_t di = getDn((opcode2 >> 12) & 7);
        int64_t value = (int64_t)di * (int64_t)src;

        /* 32 * 32 -> 64 multiply */
        if (opcode2 & (1 << 10)) {
            setDn(opcode2 & 7, value >> 32);
            setDn((opcode2 >> 12) & 7, value);
            
            if (value == 0) {
                sr |= SR_Z;
            } else if (value < 0) {
                sr |= SR_N;
            }

            SR = sr;
        }
        else 
        /* 32 * 32 -> 32 multiply */
        {
            setDn((opcode2 >> 12) & 7, value);

            /* 
                Overflow when 32 bit result sign extended to 64 bit differs 
                directly computed 64-bit result 
            */
            if (value != (int64_t)((int32_t)value)) {
                sr |= SR_Valt;
            }

            if (value == 0) {
                sr |= SR_Z;
            } else if (value < 0) {
                sr |= SR_N;
            }

            SR = sr;
        }
    }
    else {
        uint32_t src = LoadFromEA<Mode, Reg, uint32_t>();
        uint32_t di = getDn((opcode2 >> 12) & 7);
        uint64_t value = (uint64_t)di * (uint64_t)src;

        /* 32 * 32 -> 64 multiply */
        if (opcode2 & (1 << 10)) {
            setDn(opcode2 & 7, value >> 32);
            setDn((opcode2 >> 12) & 7, value);
            
            if ((int64_t)value == 0) {
                sr |= SR_Z;
            } else if ((int64_t)value < 0) {
                sr |= SR_N;
            }

            SR = sr;
        }
        else 
        /* 32 * 32 -> 32 multiply */
        {
            setDn((opcode2 >> 12) & 7, value);

            if (value >> 32) {
                sr |= SR_Valt;
            }

            if ((int64_t)value == 0) {
                sr |= SR_Z;
            } else if ((int64_t)value < 0) {
                sr |= SR_N;
            }

            SR = sr;
        }
    } 
}


#define MOVEM_L_regs_to_An_Addr(reg) \
void MOVEM_L_regs_to_A##reg##_Addr(uint32_t) \
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

MOVEM_L_regs_to_An_Addr(0);
MOVEM_L_regs_to_An_Addr(1);
MOVEM_L_regs_to_An_Addr(2);
MOVEM_L_regs_to_An_Addr(3);
MOVEM_L_regs_to_An_Addr(4);
MOVEM_L_regs_to_An_Addr(5);
MOVEM_L_regs_to_An_Addr(6);
MOVEM_L_regs_to_An_Addr(7);


#define MOVEM_L_regs_to_An_PreDec(reg) \
void MOVEM_L_regs_to_A##reg##_PreDec(uint32_t) \
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

MOVEM_L_regs_to_An_PreDec(0);
MOVEM_L_regs_to_An_PreDec(1);
MOVEM_L_regs_to_An_PreDec(2);
MOVEM_L_regs_to_An_PreDec(3);
MOVEM_L_regs_to_An_PreDec(4);
MOVEM_L_regs_to_An_PreDec(5);
MOVEM_L_regs_to_An_PreDec(6);
MOVEM_L_regs_to_An_PreDec(7);


#define MOVEM_L_regs_from_An_PostInc(reg) \
void MOVEM_L_regs_from_A##reg##_PostInc(uint32_t) \
{ \
    uint32_t *base = (uint32_t *)(uintptr_t)A##reg; \
    uint16_t mask = *(uint16_t*)(uintptr_t)(PC + 2); \
    if (mask & 0x0001) D0 = *base++; \
    if (mask & 0x0002) D1 = *base++; \
    if (mask & 0x0004) D2 = *base++; \
    if (mask & 0x0008) D3 = *base++; \
    if (mask & 0x0010) D4 = *base++; \
    if (mask & 0x0020) D5 = *base++; \
    if (mask & 0x0040) D6 = *base++; \
    if (mask & 0x0080) D7 = *base++; \
    if (mask & 0x0100) A0 = *base++; \
    if (mask & 0x0200) A1 = *base++; \
    if (mask & 0x0400) A2 = *base++; \
    if (mask & 0x0800) A3 = *base++; \
    if (mask & 0x1000) A4 = *base++; \
    if (mask & 0x2000) A5 = *base++; \
    if (mask & 0x4000) A6 = *base++; \
    if (mask & 0x8000) A7 = *base++; \
    A##reg = (uint32_t)(uintptr_t)base; \
    PC += 4; \
}

MOVEM_L_regs_from_An_PostInc(0);
MOVEM_L_regs_from_An_PostInc(1);
MOVEM_L_regs_from_An_PostInc(2);
MOVEM_L_regs_from_An_PostInc(3);
MOVEM_L_regs_from_An_PostInc(4);
MOVEM_L_regs_from_An_PostInc(5);
MOVEM_L_regs_from_An_PostInc(6);
MOVEM_L_regs_from_An_PostInc(7);

#define FILL_PEA_ALIKE(base_offset, name) \
    [&]<std::size_t... Areg>(int base, std::index_sequence<Areg...>) { \
        ((table[base + EA(2, Areg)] = \
             name<2, Areg>), ...); \
    }((base_offset), std::make_index_sequence<8>{}); \
    [&]<std::size_t... Is>(int base, std::index_sequence<Is...>) { \
        ((table[base + EA(5 + (Is >> 3), Is & 7)] = \
             name<5 + (Is >> 3), Is & 7>), ...); \
    }((base_offset), std::make_index_sequence<20>{});

#define FILL_LEA(base_offset, name, an) \
    [&]<std::size_t... Areg>(int base, std::index_sequence<Areg...>) { \
        ((table[base + EA(2, Areg)] = \
             name<2, Areg, an>), ...); \
    }((base_offset), std::make_index_sequence<8>{}); \
    [&]<std::size_t... Is>(int base, std::index_sequence<Is...>) { \
        ((table[base + EA(5 + (Is >> 3), Is & 7)] = \
             name<5 + (Is >> 3), Is & 7, an>), ...); \
    }((base_offset), std::make_index_sequence<20>{});

#define FILL_MOD0(base_offset, name) \
    [&]<std::size_t... Dreg>(int base, std::index_sequence<Dreg...>) { \
        ((table[base + EA(0, Dreg)] = \
            name<0, Dreg>), ...); \
    }((base_offset), std::make_index_sequence<8>{});

#define FILL_MOD0_size(base_offset, name, size) \
    [&]<std::size_t... Dreg>(int base, std::index_sequence<Dreg...>) { \
        ((table[base + EA(0, Dreg)] = \
            name<0, Dreg, size>), ...); \
    }((base_offset), std::make_index_sequence<8>{});

#define FILL_MOD0_reg_only(base_offset, name) \
    [&]<std::size_t... Dreg>(int base, std::index_sequence<Dreg...>) { \
        ((table[base + EA(0, Dreg)] = \
            name<Dreg>), ...); \
    }((base_offset), std::make_index_sequence<8>{});

#define FILL_MOD2_to_75(base_offset, name) \
    [&]<std::size_t... Is>(int base, std::index_sequence<Is...>) { \
        ((table[base + EA(2 + (Is >> 3), Is & 7)] = \
            name<2 + (Is >> 3), Is & 7>), ...); \
    }((base_offset), std::make_index_sequence<45>{});

#define FILL_MOD2_to_75_size(base_offset, name, size) \
    [&]<std::size_t... Is>(int base, std::index_sequence<Is...>) { \
        ((table[base + EA(2 + (Is >> 3), Is & 7)] = \
            name<2 + (Is >> 3), Is & 7, size>), ...); \
    }((base_offset), std::make_index_sequence<45>{});

#define FILL_MOD2_to_72(base_offset, name) \
    [&]<std::size_t... Is>(int base, std::index_sequence<Is...>) { \
        ((table[base + EA(2 + (Is >> 3), Is & 7)] = \
            name<2 + (Is >> 3), Is & 7>), ...); \
    }((base_offset), std::make_index_sequence<42>{});

#define FILL_MOD2_to_72_size(base_offset, name, size) \
    [&]<std::size_t... Is>(int base, std::index_sequence<Is...>) { \
        ((table[base + EA(2 + (Is >> 3), Is & 7)] = \
            name<2 + (Is >> 3), Is & 7, size>), ...); \
    }((base_offset), std::make_index_sequence<42>{});

#define FILL_ALL_RD_EAs(base_offset, name, size) \
    [&]<std::size_t... Is>(int base, std::index_sequence<Is...>) { \
        ((table[base + EA((Is >> 3), Is & 7)] = \
             name<(Is >> 3), Is & 7, size>), ...); \
    }((base_offset), std::make_index_sequence<61>{});

#define FILL_ALL_RD_EAs_no_An(base_offset, name, size) \
    [&]<std::size_t... Dreg>(int base, std::index_sequence<Dreg...>) { \
        ((table[base + EA(0, Dreg)] = \
            name<0, Dreg, size>), ...); \
    }((base_offset), std::make_index_sequence<8>{}); \
    [&]<std::size_t... Is>(int base, std::index_sequence<Is...>) { \
        ((table[base + EA(2 + (Is >> 3), Is & 7)] = \
             name<2 + (Is >> 3), Is & 7, size>), ...); \
    }((base_offset), std::make_index_sequence<45>{});

#define FILL_ALL_RD_EAs_no_An_no_size(base_offset, name) \
    [&]<std::size_t... Dreg>(int base, std::index_sequence<Dreg...>) { \
        ((table[base + EA(0, Dreg)] = \
            name<0, Dreg>), ...); \
    }((base_offset), std::make_index_sequence<8>{}); \
    [&]<std::size_t... Is>(int base, std::index_sequence<Is...>) { \
        ((table[base + EA(2 + (Is >> 3), Is & 7)] = \
             name<2 + (Is >> 3), Is & 7>), ...); \
    }((base_offset), std::make_index_sequence<45>{});

static constexpr std::array<INTERPRET_Function, 4096> BuildInsnTable()
{
    std::array<INTERPRET_Function, 4096> table{};

    auto fill = [&table](int first, int last, INTERPRET_Function func) {
        for (int i = first; i <= last; ++i)
            table[i] = func;
    };

    auto EA = [](int mode, int reg) constexpr { return (mode << 3) | reg; };

    fill(00000, 07777, UNIMPLEMENTED);

    FILL_MOD0(              02300, MOVE_to_CCR);    /* MOVE to CCR Dn */
    FILL_MOD2_to_75(        02300, MOVE_to_CCR);    /* MOVE to CCR all other modes */

    FILL_MOD0(              03300, MOVE_to_SR);     /* MOVE to SR Dn */
    FILL_MOD2_to_75(        03300, MOVE_to_SR);     /* MOVE to SR all other modes */

    FILL_MOD0_reg_only(     04100, SWAP);           /* SWAP Dn */

    FILL_MOD0_size(         01000, CLR, BYTE);      /* CLR.B Dn */
    FILL_MOD2_to_72_size(   01000, CLR, BYTE);      /* CLR.B all other modes */
    FILL_MOD0_size(         01100, CLR, WORD);      /* CLR.W Dn */
    FILL_MOD2_to_72_size(   01100, CLR, WORD);      /* CLR.W all other modes */
    FILL_MOD0_size(         01200, CLR, LONG);      /* CLR.L Dn */
    FILL_MOD2_to_72_size(   01200, CLR, LONG);      /* CLR.L all other modes */

    FILL_ALL_RD_EAs_no_An(  05000, TST, BYTE);
    FILL_ALL_RD_EAs(        05100, TST, WORD);
    FILL_ALL_RD_EAs(        05200, TST, LONG);

    FILL_MOD0_size(         02000, NEG, BYTE);      /* NOT.B Dn */
    FILL_MOD2_to_72_size(   02000, NEG, BYTE);      /* NOT.B all other modes */
    FILL_MOD0_size(         02100, NEG, WORD);      /* NOT.W Dn */
    FILL_MOD2_to_72_size(   02100, NEG, WORD);      /* NOT.W all other modes */
    FILL_MOD0_size(         02200, NEG, LONG);      /* NOT.L Dn */
    FILL_MOD2_to_72_size(   02200, NEG, LONG);      /* NOT.L all other modes */

    FILL_MOD0_size(         03000, NOT, BYTE);      /* NOT.B Dn */
    FILL_MOD2_to_72_size(   03000, NOT, BYTE);      /* NOT.B all other modes */
    FILL_MOD0_size(         03100, NOT, WORD);      /* NOT.W Dn */
    FILL_MOD2_to_72_size(   03100, NOT, WORD);      /* NOT.W all other modes */
    FILL_MOD0_size(         03200, NOT, LONG);      /* NOT.L Dn */
    FILL_MOD2_to_72_size(   03200, NOT, LONG);      /* NOT.L all other modes */

    FILL_PEA_ALIKE(         07200, JSR);            /* JSR all modes */
    FILL_PEA_ALIKE(         07300, JMP);            /* JMP all modes */
    FILL_PEA_ALIKE(         04100, PEA);            /* PEA all modes */
    FILL_LEA(               00700, LEA, 0);         /* LEA to A0 all modes */
    FILL_LEA(               01700, LEA, 1);         /* LEA to A1 all modes */
    FILL_LEA(               02700, LEA, 2);         /* LEA to A2 all modes */
    FILL_LEA(               03700, LEA, 3);         /* LEA to A3 all modes */
    FILL_LEA(               04700, LEA, 4);         /* LEA to A4 all modes */
    FILL_LEA(               05700, LEA, 5);         /* LEA to A5 all modes */
    FILL_LEA(               06700, LEA, 6);         /* LEA to A6 all modes */
    FILL_LEA(               07700, LEA, 7);         /* LEA to A7 all modes */

    FILL_MOD0_reg_only(     04010, LINK_L);         /* LINK.L #imm, An */
    FILL_MOD0_reg_only(     07120, LINK_W);         /* LINK.L #imm, An */
    FILL_MOD0_reg_only(     07130, UNLK);           /* UNLK An */

    FILL_MOD0_reg_only(     04200, EXT_B_to_W);     /* EXT.W Dn */
    FILL_MOD0_reg_only(     04300, EXT_W_to_L);     /* EXT.L Dn */
    FILL_MOD0_reg_only(     04700, EXT_B_to_L);     /* EXTB.L Dn */

    FILL_ALL_RD_EAs_no_An_no_size(  06100,  DIVU_L);
    FILL_ALL_RD_EAs_no_An_no_size(  06000,  MULU_L);

    table[04320] =     MOVEM_L_regs_to_A0_Addr;
    table[04321] =     MOVEM_L_regs_to_A1_Addr;
    table[04322] =     MOVEM_L_regs_to_A2_Addr;
    table[04323] =     MOVEM_L_regs_to_A3_Addr;
    table[04324] =     MOVEM_L_regs_to_A4_Addr;
    table[04325] =     MOVEM_L_regs_to_A5_Addr;
    table[04326] =     MOVEM_L_regs_to_A6_Addr;
    table[04327] =     MOVEM_L_regs_to_A7_Addr;

    table[06330] =     MOVEM_L_regs_from_A0_PostInc;
    table[06331] =     MOVEM_L_regs_from_A1_PostInc;
    table[06332] =     MOVEM_L_regs_from_A2_PostInc;
    table[06333] =     MOVEM_L_regs_from_A3_PostInc;
    table[06334] =     MOVEM_L_regs_from_A4_PostInc;
    table[06335] =     MOVEM_L_regs_from_A5_PostInc;
    table[06336] =     MOVEM_L_regs_from_A6_PostInc;
    table[06337] =     MOVEM_L_regs_from_A7_PostInc;

    table[04340] =     MOVEM_L_regs_to_A0_PreDec;
    table[04341] =     MOVEM_L_regs_to_A1_PreDec;
    table[04342] =     MOVEM_L_regs_to_A2_PreDec;
    table[04343] =     MOVEM_L_regs_to_A3_PreDec;
    table[04344] =     MOVEM_L_regs_to_A4_PreDec;
    table[04345] =     MOVEM_L_regs_to_A5_PreDec;
    table[04346] =     MOVEM_L_regs_to_A6_PreDec;
    table[04347] =     MOVEM_L_regs_to_A7_PreDec;

    table[07161] =     NOP;
    table[07164] =     RTD;
    table[07165] =     RTS;
    table[07167] =     RTR;
    table[07172] =     MOVEC;
    table[07173] =     MOVEC;

    fill(07100, 07117, TRAP);

    #if 0
    [00300 ... 00307] = { EMIT_MOVEfromSR, NULL, SR_ALL, 0, 1, 0, 2 },
    [00320 ... 00347] = { EMIT_MOVEfromSR, NULL, SR_ALL, 0, 1, 0, 2 },
    [00350 ... 00371] = { EMIT_MOVEfromSR, NULL, SR_ALL, 0, 1, 1, 2 },

    [01300 ... 01307] = { EMIT_MOVEfromCCR, NULL, SR_CCR, 0, 1, 0, 2 },
    [01320 ... 01347] = { EMIT_MOVEfromCCR, NULL, SR_CCR, 0, 1, 0, 2 },
    [01350 ... 01371] = { EMIT_MOVEfromCCR, NULL, SR_CCR, 0, 1, 1, 2 },
    
    [0xe70]           = { EMIT_RESET, NULL, SR_S, 0, 1, 0, 0 },
    [0xe72]           = { EMIT_STOP, NULL, SR_S, SR_ALL, 2, 0, 0 },
    [0xe73]           = { EMIT_RTE, NULL, SR_S, SR_ALL, 1, 0, 0 },
    [0xe76]           = { EMIT_TRAPV, NULL, SR_CCR, 0, 1, 0, 0 },
    
    [0xe60 ... 0xe6f] = { EMIT_MOVEUSP, NULL, SR_S, 0, 1, 0, 4 },
    [04110 ... 04117] = { EMIT_BKPT, NULL, SR_ALL, 0, 1, 0, 0 },      // BKPT

    [00000 ... 00007] = { EMIT_NEGX, NULL, SR_XZ, SR_CCR, 1, 0, 1 },
    [00100 ... 00107] = { EMIT_NEGX, NULL, SR_XZ, SR_CCR, 1, 0, 2 },
    [00200 ... 00207] = { EMIT_NEGX, NULL, SR_XZ, SR_CCR, 1, 0, 4 },

    [00020 ... 00047] = { EMIT_NEGX, NULL, SR_XZ, SR_CCR, 1, 0, 1 },
    [00120 ... 00147] = { EMIT_NEGX, NULL, SR_XZ, SR_CCR, 1, 0, 2 },
    [00220 ... 00247] = { EMIT_NEGX, NULL, SR_XZ, SR_CCR, 1, 0, 4 },
    
    [00050 ... 00071] = { EMIT_NEGX, NULL, SR_XZ, SR_CCR, 1, 1, 1 },
    [00150 ... 00171] = { EMIT_NEGX, NULL, SR_XZ, SR_CCR, 1, 1, 2 },
    [00250 ... 00271] = { EMIT_NEGX, NULL, SR_XZ, SR_CCR, 1, 1, 4 },

    [04000 ... 04007] = { EMIT_NBCD, NULL, SR_XZ, SR_XZC, 1, 0, 1 },
    [04020 ... 04047] = { EMIT_NBCD, NULL, SR_XZ, SR_XZC, 1, 0, 1 },
    [04050 ... 04071] = { EMIT_NBCD, NULL, SR_XZ, SR_XZC, 1, 1, 1 },

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

} // Emu68::M68k::Interpreter

static constexpr auto InsnTable = Emu68::M68k::Interpreter::BuildInsnTable();

__attribute__((optimize("no-optimize-sibling-calls")))
void INTERPRET_line4(uint32_t opcode)
{
    InsnTable[opcode & 0xfff](opcode);
}
