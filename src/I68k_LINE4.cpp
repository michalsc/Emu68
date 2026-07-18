#include <stdint.h>
#include <arm_neon.h>
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

register uint32_t PC asm("w18");
register uint32_t D0 asm("w19");
register uint32_t D1 asm("w20");
register uint32_t D2 asm("w21");
register uint32_t D3 asm("w22");
register uint32_t D4 asm("w23");
register uint32_t D5 asm("w24");
register uint32_t D6 asm("w25");
register uint32_t D7 asm("w26");

register uint32_t A0 asm("w13");
register uint32_t A1 asm("w14");
register uint32_t A2 asm("w15");
register uint32_t A3 asm("w16");
register uint32_t A4 asm("w17");
register uint32_t A5 asm("w27");
register uint32_t A6 asm("w28");
register uint32_t A7 asm("w29");

static inline struct M68KState *getCTX()
{
    struct M68KState *ctx;
    __asm__ volatile("mov %0, " CTX_POINTER_ASM:"=r"(ctx));
    return ctx;
}

#if 1

static inline uint32_t getSR()
{
    uint32_t sr;
    __asm__ volatile("umov %w0, " REG_SR_ASM:"=r"(sr));
    return sr;
}

static inline void setSR(uint32_t sr)
{
    __asm__ volatile("mov " REG_SR_ASM ", %w0": :"r"(sr));
}

#else

static inline uint16_t getSR(void) {
    return vgetq_lane_u16(vreinterpretq_u16_u32(reg_v19), 5);
}
static inline void setSR(uint16_t v) {
    reg_v19 = vreinterpretq_u32_u16(vsetq_lane_u16(v, vreinterpretq_u16_u32(reg_v19), 5));
}

#endif

#define INTERPRET_SWAP_Dn(dn) \
uint32_t INTERPRET_SWAP_D##dn(uint16_t) \
{ \
    uint16_t sr = getSR() & 0xfff0; \
    \
    if (unlikely(D##dn == 0)) { \
        sr |= SR_Z; \
    } else { \
        D##dn = (D##dn >> 16) | (D##dn << 16); \
        if (D##dn & 0x80000000) { \
            sr = SR_N; \
        } \
    } \
    setSR(sr); \
    PC += 2; \
    return 1; \
}

INTERPRET_SWAP_Dn(0)
INTERPRET_SWAP_Dn(1)
INTERPRET_SWAP_Dn(2)
INTERPRET_SWAP_Dn(3)
INTERPRET_SWAP_Dn(4)
INTERPRET_SWAP_Dn(5)
INTERPRET_SWAP_Dn(6)
INTERPRET_SWAP_Dn(7)
#if 0
template <int N> inline uint32_t getD();
template <int N> inline void    setD(uint32_t v);

template <> inline uint32_t getD<0>()          { return D0; }
template <> inline void    setD<0>(uint32_t v) { D0 = v; }
template <> inline uint32_t getD<1>()          { return D1; }
template <> inline void    setD<1>(uint32_t v) { D1 = v; }
template <> inline uint32_t getD<2>()          { return D2; }
template <> inline void    setD<2>(uint32_t v) { D2 = v; }
template <> inline uint32_t getD<3>()          { return D3; }
template <> inline void    setD<3>(uint32_t v) { D3 = v; }
template <> inline uint32_t getD<4>()          { return D4; }
template <> inline void    setD<4>(uint32_t v) { D4 = v; }
template <> inline uint32_t getD<5>()          { return D5; }
template <> inline void    setD<5>(uint32_t v) { D5 = v; }
template <> inline uint32_t getD<6>()          { return D6; }
template <> inline void    setD<6>(uint32_t v) { D6 = v; }
template <> inline uint32_t getD<7>()          { return D7; }
template <> inline void    setD<7>(uint32_t v) { D7 = v; }

template <int dn>
void INTERPRET_SWAP_D(uint16_t)
{
    uint32_t v = getD<dn>();
    uint16_t sr = getSR() & 0xfff0;

    if (unlikely(v == 0)) {
        sr |= SR_Z;
    } else {
        v = (v >> 16) | (v << 16);
        setD<dn>(v);
        if (v & 0x80000000) {
            sr = SR_N;
        }
    }
    setSR(sr);
    PC += 2;
}

void __attribute__((used)) INTERPRET_SWAP_D0_(uint16_t opcode) { INTERPRET_SWAP_D<0>(opcode); }
#endif

#define INTERPRET_CLR_B_Dn(dn) \
void INTERPRET_CLR_B_D##dn(uint16_t) \
{ \
    uint16_t sr = getSR() & 0xfff0; \
    D##dn &= 0xffffff00; \
    sr &= ~(SR_N | SR_Z | SR_V | SR_C); \
    sr |= SR_Z; \
    setSR(sr); \
    PC += 2; \
    bumpINSN_COUNT(); }

INTERPRET_CLR_B_Dn(0)
INTERPRET_CLR_B_Dn(1)
INTERPRET_CLR_B_Dn(2)
INTERPRET_CLR_B_Dn(3)
INTERPRET_CLR_B_Dn(4)
INTERPRET_CLR_B_Dn(5)
INTERPRET_CLR_B_Dn(6)
INTERPRET_CLR_B_Dn(7)
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

void INTERPRET_RTS_(uint16_t)
{
    PC = *(uint32_t *)(uintptr_t)A7;
    A7 += 4;
}
#endif