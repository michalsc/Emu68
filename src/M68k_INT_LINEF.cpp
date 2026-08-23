#include <cstdint>
#include <arm_neon.h>
#include <array>
#include <limits>

#include <emu68/m68k/interpreter>

#include "RegisterMapping.h"

extern "C" {

#include "math/libm.h"
#include "cache.h"

uint64_t Load96bit(uintptr_t __ignore, uintptr_t base);
uint64_t Store96bit(uint64_t value, uintptr_t base);
void invalidate_entire_dcache(void);
extern int dcache_mask_bits;
extern uint32_t EPOCH;

typedef union 
{
    uint8_t  c[12];
    uint32_t i[3];
} packed_t;

double PackedToDouble(packed_t value);
packed_t DoubleToPacked(double value, int k);

void M68K_LoadContext(struct M68KState *ctx);
void M68K_SaveContext(struct M68KState *ctx);

static inline struct M68KState *getCTX()
{
    struct M68KState *ctx;
    __asm__ volatile("mov %0, " CTX_POINTER_ASM:"=r"(ctx));
    return ctx;
}

}

namespace Emu68::M68k::Interpreter::LineF {

[[gnu::noinline]] void resetFPUState()
{
    union {
        uint64_t u64;
        double d;
    } nan_union;

    /* Quiet NaN as used by 68881/2 */
    nan_union.u64 = 0x7fffffffffffffffULL;

    FP0 = nan_union.d;
    FP1 = nan_union.d;
    FP2 = nan_union.d;
    FP3 = nan_union.d;
    FP4 = nan_union.d;
    FP5 = nan_union.d;
    FP6 = nan_union.d;
    FP7 = nan_union.d;

    FPCR = 0;
    FPSR = 0;
    FPIAR = 0;

    asm volatile("msr FPCR, %0"::"r"(0));
}

[[gnu::noinline]] void updateFlagsFPU(double value)
{
    uint32_t fpsr = FPSR;
    uint64_t tmp;
    asm volatile(
        "fcmp   %d2, #0.0           \n\t"
        "bic    %w0,%w0,0x0f000000  \n\t"
        "mrs    %1, NZCV            \n\t"
        "bic    %w1, %w1, 0x20000000  \n\t"
        "orr    %w0, %w0, %w1, LSR #4  \n\t"
        :"=r"(fpsr),"=r"(tmp):"y"(value),"0"(fpsr):"cc");
    FPSR = fpsr;
}

[[gnu::always_inline]] inline double roundToInt(double value)
{
    double result;
    asm volatile("frinti %d0, %d1":"=w"(result):"w"(value));
    return result;
}

[[gnu::always_inline]] inline double roundToIntZero(double value)
{
    double result;
    asm volatile("frintz %d0, %d1":"=w"(result):"w"(value));
    return result;
}

[[gnu::always_inline]] inline double roundToSingle(double value)
{
    float f;
    asm volatile("fcvt %s0, %d1" : "=w"(f) : "w"(value));
    double result;
    asm volatile("fcvt %d0, %s1" : "=w"(result) : "w"(f));
    return result;
}

template<uint8_t Mode = DEFAULT_EA, uint8_t Reg = DEFAULT_EA>
void FSAVE(uint32_t opcode)
{
    uint8_t mode = (Mode == DEFAULT_EA) ? ((opcode >> 3) & 7) : Mode;
    uint8_t reg = (Reg == DEFAULT_EA) ? (opcode & 7) : Reg;

    advancePC(2);

    /* Save only IDLE frame, we not need to record any changes here */
    if constexpr (Mode == DEFAULT_EA || Reg == DEFAULT_EA) { 
        storeToEA<uint32_t>(mode, reg, 0x41000000);
    } else {
        storeToEA<Mode, Reg, uint32_t>(0x41000000);
    }
}

template<uint8_t Mode = DEFAULT_EA, uint8_t Reg = DEFAULT_EA>
void FRESTORE(uint32_t opcode)
{
    uint32_t state;
    uint8_t mode = (Mode == DEFAULT_EA) ? ((opcode >> 3) & 7) : Mode;
    uint8_t reg = (Reg == DEFAULT_EA) ? (opcode & 7) : Reg;

    advancePC(2);

    /* Restore frame header */
    if constexpr (Mode == DEFAULT_EA || Reg == DEFAULT_EA) {
        state = loadFromEA<LONG>(mode, reg);
    } else { 
        state = loadFromEA<Mode, Reg, uint32_t>();
    }

    /* If restored NULL frame, reset FPU state */
    if ((state & 0xff000000) == 0) {
        resetFPUState();
    }
}

void ILLEGAL_LINE_F(uint32_t)
{
    raiseException(VECTOR_LINE_F, ExceptionFrameFormat::FORMAT_0, 0, 0);
}

bool ILLEGAL_LINE_F(uint32_t opcode, uint32_t)
{
    advancePC(-4);

    ILLEGAL_LINE_F(opcode);

    return 0;
}

[[gnu::noinline]] bool loadFromEA(uint32_t mode, uint32_t reg, uint32_t format, double& out)
{
    union {
        uint64_t u64;
        double d;
    } u;
    packed_t p;
    uint32_t addr = 0;
    uint8_t size = 0;

    switch (format) {
        case 0: size = 4; break;    /* int */
        case 1: size = 4; break;    /* single */
        case 2: size = 12; break;   /* extended */
        case 3: size = 12; break;   /* packed */
        case 4: size = 2; break;    /* short */
        case 5: size = 8; break;    /* double */
        case 6: size = 1; break;    /* char */
    }

    /* Load from Dn allowed only if type is char, short, int or single */
    if (mode == 0 && (format == 0 || format == 1 || format == 4 || format == 6)) {
        union {
            uint32_t u32;
            float f;
        } u32;
        switch (format) {
            case 0: out = (double)getD<int32_t>(reg); return true;
            case 1: u32.u32 = getD<int32_t>(reg); out = u32.f; return true;
            case 4: out = (double)getD<int16_t>(reg); return true;
            case 6: out = (double)getD<int8_t>(reg); return true;
        }
    }

    /* Anything else with mode 0 or mode 1 is a reserved/illegal encoding. */
    if (mode < 2) {
        return false;
    }

    switch (size)
    {
        case 1: addr = getEA<uint8_t>(mode, reg); break;
        case 2: addr = getEA<uint16_t>(mode, reg); break;
        case 4: addr = getEA<float>(mode, reg); break;
        case 8: addr = getEA<double>(mode, reg); break;
        case 12: addr = getEA<uint32_t[3]>(mode, reg); break;
    }

    switch (format) {
        case 0: out = *(int32_t *)(uintptr_t)addr; break;
        case 1: out = *(float *)(uintptr_t)addr; break;
        case 2: u.u64 = Load96bit(0, addr); out = u.d; break;
        case 3: p = *(packed_t *)(uintptr_t)addr; out = PackedToDouble(p); break;
        case 4: out = *(int16_t *)(uintptr_t)addr; break;
        case 5: out = *(double *)(uintptr_t)addr; break;
        case 6: out = *(int8_t *)(uintptr_t)addr; break;
    }

    return true;
}

template<bool LongJump, uint8_t InstCC>
void FBcc(uint32_t)
{
    uint32_t pc = getPC<uint32_t>(2);
    uint32_t next_pc = pc;
    int32_t bra_off;

    if constexpr (LongJump) {
        bra_off = *(int32_t*)(uintptr_t)(pc);
        next_pc += 4;
    } else {
        bra_off = *(int16_t*)(uintptr_t)(pc);
        next_pc += 2;
    }

    if (evalCondFPU<InstCC>()) {
        pc += bra_off;
    } else {
        pc = next_pc;
    }

    setPC(pc);
}

template<bool RegToReg, uint8_t DstReg, uint8_t SrcReg, bool UpdateDst = true, class Fn>
bool MONADIC(uint32_t opcode, uint32_t opcode2, Fn&& modify)
{
    uint8_t src = (SrcReg == DEFAULT_EA) ? (opcode2 >> 10) & 7 : SrcReg;
    uint8_t dst = (DstReg == DEFAULT_EA) ? (opcode2 >> 7) & 7 : DstReg;
    double value;

    if constexpr (RegToReg) {
        if constexpr (DstReg == DEFAULT_EA) {
            value = modify(getFP<double>(src));
            if constexpr (UpdateDst) { setFP<double>(dst, value); }
        } else if constexpr (SrcReg == DEFAULT_EA) {
            value = modify(getFP<double>(src));
            if constexpr (UpdateDst) { setFP<DstReg, double>(value); }
        } else {
            value = modify(getFP<SrcReg, double>());
            if constexpr (UpdateDst) { setFP<DstReg, double>(value); }
        }
    } else {
        uint32_t mode = (opcode >> 3) & 7;
        uint32_t reg = opcode & 7;
        uint32_t format = (opcode2 >> 10) & 7;

        if (!loadFromEA(mode, reg, format, value)) { return false; }
        
        value = modify(value);

        if constexpr (DstReg == DEFAULT_EA) {
            if constexpr (UpdateDst) setFP<double>(dst, value);
        } else {
            if constexpr (UpdateDst) setFP<DstReg, double>(value);
        }
    }

    updateFlagsFPU(value);

    return true;
}

template<bool RegToReg, uint8_t DstReg, uint8_t SrcReg, bool UpdateDst = true, class Fn>
bool DYADIC(uint32_t opcode, uint32_t opcode2, Fn&& modify)
{
    uint8_t src = (SrcReg == DEFAULT_EA) ? (opcode2 >> 10) & 7 : SrcReg;
    uint8_t dst = (DstReg == DEFAULT_EA) ? (opcode2 >> 7) & 7 : DstReg;
    double value;

    if constexpr (RegToReg) {
        if constexpr (DstReg == DEFAULT_EA) {
            value = modify(getFP<double>(src), getFP<double>(dst));
            if constexpr (UpdateDst) { setFP<double>(dst, value); }
        } else if constexpr (SrcReg == DEFAULT_EA) {
            value = modify(getFP<double>(src), getFP<DstReg, double>());
            if constexpr (UpdateDst) { setFP<DstReg, double>(value); }
        } else {
            value = modify(getFP<SrcReg, double>(), getFP<DstReg, double>());
            if constexpr (UpdateDst) { setFP<DstReg, double>(value); }
        }
    } else {
        uint32_t mode = (opcode >> 3) & 7;
        uint32_t reg = opcode & 7;
        uint32_t format = (opcode2 >> 10) & 7;

        if (!loadFromEA(mode, reg, format, value)) { return false; }

        if constexpr (DstReg == DEFAULT_EA) {
            value = modify(value, getFP<double>(dst));
            if constexpr (UpdateDst) { setFP<double>(dst, value); }
        } else {
            value = modify(value, getFP<DstReg, double>());
            if constexpr (UpdateDst) { setFP<DstReg, double>(value); }
        }
    }

    updateFlagsFPU(value);

    return true;
}

/* Dyadic operations */

template<bool RegToReg, uint8_t DstReg = DEFAULT_EA, uint8_t SrcReg = DEFAULT_EA, bool SingleRounding = false>
bool FADD(uint32_t opcode, uint32_t opcode2)
{
    return DYADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double op1, double op2) -> double { 
        double result = op1 + op2; 
        if constexpr (SingleRounding) { result = roundToSingle(result); }
        return result;
    });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT_EA, uint8_t SrcReg = DEFAULT_EA>
bool FCMP(uint32_t opcode, uint32_t opcode2)
{
    return DYADIC<RegToReg, DstReg, SrcReg, false>(opcode, opcode2, [](double op1, double op2) -> double { return op2 - op1; });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT_EA, uint8_t SrcReg = DEFAULT_EA, bool SingleRounding = false>
bool FDIV(uint32_t opcode, uint32_t opcode2)
{
    return DYADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double op1, double op2) -> double { 
        double result = op2 / op1; 
        if constexpr (SingleRounding) { result = roundToSingle(result); }
        return result;
    });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT_EA, uint8_t SrcReg = DEFAULT_EA, bool SingleRounding = false>
bool FSGLDIV(uint32_t opcode, uint32_t opcode2)
{
    return DYADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double op1, double op2) -> double { 
        double result = op2 / op1; 
        return roundToSingle(result);
    });
}

// FMOD

template<bool RegToReg, uint8_t DstReg = DEFAULT_EA, uint8_t SrcReg = DEFAULT_EA, bool SingleRounding = false>
bool FMUL(uint32_t opcode, uint32_t opcode2)
{
    return DYADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double op1, double op2) -> double { 
        double result = op2 * op1; 
        if constexpr (SingleRounding) { result = roundToSingle(result); }
        return result;
    });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT_EA, uint8_t SrcReg = DEFAULT_EA, bool SingleRounding = false>
bool FSGLMUL(uint32_t opcode, uint32_t opcode2)
{
    return DYADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double op1, double op2) -> double { 
        double result = op2 * op1; 
        return roundToSingle(result);
    });
}

// FREM

// FSCALE

template<bool RegToReg, uint8_t DstReg = DEFAULT_EA, uint8_t SrcReg = DEFAULT_EA, bool SingleRounding = false>
bool FSUB(uint32_t opcode, uint32_t opcode2)
{
    return DYADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double op1, double op2) -> double { 
        double result = op2 - op1; 
        if constexpr (SingleRounding) { result = roundToSingle(result); }
        return result;
    });
}

/* Monadic operations */

template<bool RegToReg, uint8_t DstReg = DEFAULT_EA, uint8_t SrcReg = DEFAULT_EA>
bool FABS(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { return fabs(value); });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT_EA, uint8_t SrcReg = DEFAULT_EA>
bool FACOS(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { return acos(value); });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT_EA, uint8_t SrcReg = DEFAULT_EA>
bool FASIN(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { return asin(value); });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT_EA, uint8_t SrcReg = DEFAULT_EA>
bool FATAN(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { return atan(value); });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT_EA, uint8_t SrcReg = DEFAULT_EA>
bool FATANH(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { return atanh(value); });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT_EA, uint8_t SrcReg = DEFAULT_EA>
bool FCOS(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { return cos(value); });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT_EA, uint8_t SrcReg = DEFAULT_EA>
bool FCOSH(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { return cosh(value); });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT_EA, uint8_t SrcReg = DEFAULT_EA>
bool FETOX(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { return exp(value); });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT_EA, uint8_t SrcReg = DEFAULT_EA>
bool FETOXM1(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { return expm1(value); });
}

// FGETEXP

// FGETMAN

template<bool RegToReg, uint8_t DstReg = DEFAULT_EA, uint8_t SrcReg = DEFAULT_EA>
bool FINT(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { return roundToInt(value); });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT_EA, uint8_t SrcReg = DEFAULT_EA>
bool FINTRZ(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { return roundToIntZero(value); });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT_EA, uint8_t SrcReg = DEFAULT_EA>
bool FLOG10(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { return log10(value); });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT_EA, uint8_t SrcReg = DEFAULT_EA>
bool FLOG2(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { return log2(value); });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT_EA, uint8_t SrcReg = DEFAULT_EA>
bool FLOGN(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { return log(value); });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT_EA, uint8_t SrcReg = DEFAULT_EA>
bool FLOGNP1(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { return log1p(value); });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT_EA, uint8_t SrcReg = DEFAULT_EA, bool SingleRounding = false>
bool FMOVE(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { 
        if constexpr (SingleRounding) { return roundToSingle(value); }
        else { return value; }
    });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT_EA, uint8_t SrcReg = DEFAULT_EA, bool SingleRounding = false>
bool FNEG(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { 
        if constexpr (SingleRounding) { return roundToSingle(-value); }
        else { return -value; }
    });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT_EA, uint8_t SrcReg = DEFAULT_EA>
bool FSIN(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { return sin(value); });
}

// FSINCOS - special case!

template<bool RegToReg, uint8_t DstReg = DEFAULT_EA, uint8_t SrcReg = DEFAULT_EA>
bool FSINH(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { return sinh(value); });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT_EA, uint8_t SrcReg = DEFAULT_EA>
bool FSQRT(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { return sqrt(value); });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT_EA, uint8_t SrcReg = DEFAULT_EA>
bool FTAN(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { return tan(value); });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT_EA, uint8_t SrcReg = DEFAULT_EA>
bool FTANH(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { return tanh(value); });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT_EA, uint8_t SrcReg = DEFAULT_EA>
bool FTENTOX(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { return exp10(value); });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT_EA, uint8_t SrcReg = DEFAULT_EA>
bool FTST(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg, false>(opcode, opcode2, [](double value) -> double { return value; });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT_EA, uint8_t SrcReg = DEFAULT_EA>
bool FTWOTOX(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { return exp2(value); });
}

bool FMOVECR(uint8_t ry, uint8_t extension)
{
    setFP<double>(ry, FPUConstants[extension]);

    return true;
}

bool FMOVEM_to_EA(uint32_t opcode, uint32_t opcode2)
{
    uint8_t reglist = opcode2 & 0xff;
    uint8_t mode = (opcode >> 3) & 7;
    uint8_t reg = opcode & 7;
    bool postinc_mode = (opcode2 & (1 << 12)) != 0;
    uint32_t addr;
    union {
        uint64_t u64;
        double d;
    } f64;

    /* Dn, An, (An)+ not supported */
    if (mode < 2 || mode == 3) {
        return false;
    }
    
    /* PC-Relative not supported */
    if (mode == 7 && reg > 1) {
        return false;
    }

    /* Dynamic register list? Get registers from integer reg */
    if ((opcode2 & (1 << 11)) != 0) {
        reglist = getD<uint8_t>((reglist >> 4) & 7);
    }
    
    /* Predecrement mode is different as we need to update the address */
    if (mode == 4) {
        addr = getA<uint32_t>(reg);

        if (postinc_mode) {
            if (reglist & 0b10000000) { addr -= 12; f64.d = FP0; Store96bit(f64.u64, addr); }
            if (reglist & 0b01000000) { addr -= 12; f64.d = FP1; Store96bit(f64.u64, addr); }
            if (reglist & 0b00100000) { addr -= 12; f64.d = FP2; Store96bit(f64.u64, addr); }
            if (reglist & 0b00010000) { addr -= 12; f64.d = FP3; Store96bit(f64.u64, addr); }
            if (reglist & 0b00001000) { addr -= 12; f64.d = FP4; Store96bit(f64.u64, addr); }
            if (reglist & 0b00000100) { addr -= 12; f64.d = FP5; Store96bit(f64.u64, addr); }
            if (reglist & 0b00000010) { addr -= 12; f64.d = FP6; Store96bit(f64.u64, addr); }
            if (reglist & 0b00000001) { addr -= 12; f64.d = FP7; Store96bit(f64.u64, addr); }
        } else {
            if (reglist & 0b10000000) { addr -= 12; f64.d = FP7; Store96bit(f64.u64, addr); }
            if (reglist & 0b01000000) { addr -= 12; f64.d = FP6; Store96bit(f64.u64, addr); }
            if (reglist & 0b00100000) { addr -= 12; f64.d = FP5; Store96bit(f64.u64, addr); }
            if (reglist & 0b00010000) { addr -= 12; f64.d = FP4; Store96bit(f64.u64, addr); }
            if (reglist & 0b00001000) { addr -= 12; f64.d = FP3; Store96bit(f64.u64, addr); }
            if (reglist & 0b00000100) { addr -= 12; f64.d = FP2; Store96bit(f64.u64, addr); }
            if (reglist & 0b00000010) { addr -= 12; f64.d = FP1; Store96bit(f64.u64, addr); }
            if (reglist & 0b00000001) { addr -= 12; f64.d = FP0; Store96bit(f64.u64, addr); }
        }

        setA<uint32_t>(reg, addr);
    } else {
        addr = getEA<void>(mode, reg);
        if (postinc_mode) {
            if (reglist & 0b10000000) { f64.d = FP0; Store96bit(f64.u64, addr); addr += 12; }
            if (reglist & 0b01000000) { f64.d = FP1; Store96bit(f64.u64, addr); addr += 12; }
            if (reglist & 0b00100000) { f64.d = FP2; Store96bit(f64.u64, addr); addr += 12; }
            if (reglist & 0b00010000) { f64.d = FP3; Store96bit(f64.u64, addr); addr += 12; }
            if (reglist & 0b00001000) { f64.d = FP4; Store96bit(f64.u64, addr); addr += 12; }
            if (reglist & 0b00000100) { f64.d = FP5; Store96bit(f64.u64, addr); addr += 12; }
            if (reglist & 0b00000010) { f64.d = FP6; Store96bit(f64.u64, addr); addr += 12; }
            if (reglist & 0b00000001) { f64.d = FP7; Store96bit(f64.u64, addr); addr += 12; }
        } else {
            if (reglist & 0b10000000) { f64.d = FP7; Store96bit(f64.u64, addr); addr += 12; }
            if (reglist & 0b01000000) { f64.d = FP6; Store96bit(f64.u64, addr); addr += 12; }
            if (reglist & 0b00100000) { f64.d = FP5; Store96bit(f64.u64, addr); addr += 12; }
            if (reglist & 0b00010000) { f64.d = FP4; Store96bit(f64.u64, addr); addr += 12; }
            if (reglist & 0b00001000) { f64.d = FP3; Store96bit(f64.u64, addr); addr += 12; }
            if (reglist & 0b00000100) { f64.d = FP2; Store96bit(f64.u64, addr); addr += 12; }
            if (reglist & 0b00000010) { f64.d = FP1; Store96bit(f64.u64, addr); addr += 12; }
            if (reglist & 0b00000001) { f64.d = FP0; Store96bit(f64.u64, addr); addr += 12; }
        }
    }

    return true;
}

bool FMOVEM_from_EA(uint32_t opcode, uint32_t opcode2)
{
    uint8_t reglist = opcode2 & 0xff;
    uint8_t mode = (opcode >> 3) & 7;
    uint8_t reg = opcode & 7;
    bool postinc_mode = (opcode2 & (1 << 12)) != 0;
    uint32_t addr;
    union {
        uint64_t u64;
        double d;
    } f64;

    /* Dn, An, -(An) not supported */
    if (mode < 2 || mode == 4) {
        return false;
    }
    
    /* Immediate not supported */
    if (mode == 7 && reg > 3) {
        return false;
    }

    /* Dynamic register list? Get registers from integer reg */
    if ((opcode2 & (1 << 11)) != 0) {
        reglist = getD<uint8_t>((reglist >> 4) & 7);
    }
    
    addr = getEA<void>(mode, reg);

    if (postinc_mode) {
        if (reglist & 0b10000000) { f64.u64 = Load96bit(0, addr); FP0 = f64.d; addr += 12; }
        if (reglist & 0b01000000) { f64.u64 = Load96bit(0, addr); FP1 = f64.d; addr += 12; }
        if (reglist & 0b00100000) { f64.u64 = Load96bit(0, addr); FP2 = f64.d; addr += 12; }
        if (reglist & 0b00010000) { f64.u64 = Load96bit(0, addr); FP3 = f64.d; addr += 12; }
        if (reglist & 0b00001000) { f64.u64 = Load96bit(0, addr); FP4 = f64.d; addr += 12; }
        if (reglist & 0b00000100) { f64.u64 = Load96bit(0, addr); FP5 = f64.d; addr += 12; }
        if (reglist & 0b00000010) { f64.u64 = Load96bit(0, addr); FP6 = f64.d; addr += 12; }
        if (reglist & 0b00000001) { f64.u64 = Load96bit(0, addr); FP7 = f64.d; addr += 12; }
    } else {
        if (reglist & 0b10000000) { f64.u64 = Load96bit(0, addr); FP7 = f64.d; addr += 12; }
        if (reglist & 0b01000000) { f64.u64 = Load96bit(0, addr); FP6 = f64.d; addr += 12; }
        if (reglist & 0b00100000) { f64.u64 = Load96bit(0, addr); FP5 = f64.d; addr += 12; }
        if (reglist & 0b00010000) { f64.u64 = Load96bit(0, addr); FP4 = f64.d; addr += 12; }
        if (reglist & 0b00001000) { f64.u64 = Load96bit(0, addr); FP3 = f64.d; addr += 12; }
        if (reglist & 0b00000100) { f64.u64 = Load96bit(0, addr); FP2 = f64.d; addr += 12; }
        if (reglist & 0b00000010) { f64.u64 = Load96bit(0, addr); FP1 = f64.d; addr += 12; }
        if (reglist & 0b00000001) { f64.u64 = Load96bit(0, addr); FP0 = f64.d; addr += 12; }
    }
    
    /* Postincrement mode: update the address */
    if (mode == 3) {
        setA<uint32_t>(reg, addr);
    }

    return true;
}

bool FMOVE_Sys_to_EA(uint32_t opcode, uint32_t opcode2)
{
    uint8_t reglist = (opcode2 >> 10) & 7;
    uint8_t mode = (opcode >> 3) & 7;
    uint8_t reg = opcode & 7;
    bool handled = true;

    if (reglist == 0) { return false; }

    /* Dn mode allows moving only one system register to Dn */
    if (mode == 0) {
        switch (reglist) {
            case 4: setD<uint32_t>(reg, FPCR); break;
            case 2: setD<uint32_t>(reg, FPSR); break;
            case 1: setD<uint32_t>(reg, FPIAR); break;
            default: handled = false;
        }
    } else if (mode == 1) {
        if (reglist == 1) setA<uint32_t>(reg, FPIAR);
        else handled = false;
    } else {
        uint32_t size = 0;
        uint32_t addr = 0;

        if (reglist & 1) size += 4;
        if (reglist & 2) size += 4;
        if (reglist & 4) size += 4;

        switch (size) {
            case 4: addr = getEA<uint32_t>(mode, reg); break;
            case 8: addr = getEA<uint32_t[2]>(mode, reg); break;
            case 12: addr = getEA<uint32_t[3]>(mode, reg); break;
        }

        if (reglist & 4) { *(uint32_t*)(uintptr_t)addr = FPCR; addr += 4; }
        if (reglist & 2) { *(uint32_t*)(uintptr_t)addr = FPSR; addr += 4; }
        if (reglist & 1) { *(uint32_t*)(uintptr_t)addr = FPIAR; addr += 4; }
    }

    return handled;
}

bool FMOVE_EA_to_Sys(uint32_t opcode, uint32_t opcode2)
{
    uint8_t reglist = (opcode2 >> 10) & 7;
    uint8_t mode = (opcode >> 3) & 7;
    uint8_t reg = opcode & 7;
    bool handled = true;

    if (reglist == 0) { return false; }

    /* Dn mode allows moving only one system register to Dn */
    if (mode == 0) {
        switch (reglist) {
            case 4: FPCR = getD<uint32_t>(reg); break;
            case 2: FPSR = getD<uint32_t>(reg); break;
            case 1: FPIAR = getD<uint32_t>(reg); break;
            default: handled = false;
        }
    } else if (mode == 1) {
        if (reglist == 1) FPIAR = getA<uint32_t>(reg);
        else handled = false;
    } else {
        uint32_t size = 0;
        uint32_t addr = 0;

        if (reglist & 1) size += 4;
        if (reglist & 2) size += 4;
        if (reglist & 4) size += 4;

        switch (size) {
            case 4: addr = getEA<uint32_t>(mode, reg); break;
            case 8: addr = getEA<uint32_t[2]>(mode, reg); break;
            case 12: addr = getEA<uint32_t[3]>(mode, reg); break;
        }

        if (reglist & 4) { FPCR = *(uint32_t*)(uintptr_t)addr; addr += 4; }
        if (reglist & 2) { FPSR = *(uint32_t*)(uintptr_t)addr; addr += 4; }
        if (reglist & 1) { FPIAR = *(uint32_t*)(uintptr_t)addr; addr += 4; }
    }

    /* If instruction was handled and FPCR was updated, set ARM fpcr to adjust rounding */
    if (handled && (reglist & 4)) {
        uint64_t arm_fpcr;
        uint32_t fpcr = (FPCR >> 4) & 3; /* Extract rounding bits from m68k FPCR */

        asm volatile("mrs %0, FPCR":"=r"(arm_fpcr));

        /* Invert bits accordingly */
        fpcr = (-fpcr + 4) & 3;

        /* Insert to ARM fpcr and update it*/
        arm_fpcr = (arm_fpcr & ~(3 << 22)) | fpcr << 22;

        asm volatile("msr FPCR, %0"::"r"(arm_fpcr));
    }

    return handled;
}

bool FMOVE_to_EA(uint32_t opcode, uint32_t opcode2)
{
    uint8_t mode = (opcode >> 3) & 7;
    uint8_t reg = opcode & 7;
    uint8_t format = (opcode2 >> 10) & 7;
    int k;
    union {
        uint64_t u64;
        double d;
    } f64;
    union {
        uint32_t u32;
        float f;
    } f32;

    double value = getFP<double>((opcode2 >> 7) & 7);

    if (mode == 1) { return false; }

    if (mode == 0 && (format == 0 || format == 1 || format == 4 || format == 6)) {
        switch (format) {
            case 0: setD<int32_t>(reg, (int32_t)value); break;
            case 1: f32.f = value; setD<uint32_t>(reg, f32.u32); break;
            case 4: setD<int16_t>(reg, (int16_t)value); break;
            case 6: setD<int8_t>(reg, (int8_t)value); break;
        }
        return true;
    } else {
        if (mode < 2) { return false; }

        switch (format) {
            case 0: 
                *(int32_t*)(uintptr_t)getEA<uint32_t>(mode, reg) = (int32_t)value; 
                break;
            case 1: 
                *(float*)(uintptr_t)getEA<float>(mode, reg) = (float)value;
                break;
            case 2: 
                f64.d = value;
                Store96bit(f64.u64, getEA<uint32_t[3]>(mode, reg));
                break;
            case 3:
                k = opcode2 & 0x7f;
                *(packed_t*)(uintptr_t)getEA<uint32_t[3]>(mode, reg) = DoubleToPacked(value, k);
                break;
            case 4: 
                *(uint16_t*)(uintptr_t)getEA<uint16_t>(mode, reg) = (int16_t)value;
                break;
            case 5: 
                *(double*)(uintptr_t)getEA<double>(mode, reg) = value;
                break;
            case 6:
                *(int8_t*)(uintptr_t)getEA<uint8_t>(mode, reg) = (int8_t)value; 
                break;
            case 7: 
                k = getD<uint8_t>((opcode2 >> 4) & 7); 
                *(packed_t*)(uintptr_t)getEA<uint32_t[3]>(mode, reg) = DoubleToPacked(value, k);
                break;
        }
    }

    return true;
}

template<bool RegToReg>
static consteval std::array<INTERPRET_FPU_Function, 128> buildExtensionFieldService()
{
    std::array<INTERPRET_FPU_Function, 128> table{};

    for (int i = 0; i < 128; ++i) {
        table[i] = ILLEGAL_LINE_F;
    }

    table[0x00] = FMOVE<RegToReg>;
    table[0x01] = FINT<RegToReg>;
    table[0x02] = FSINH<RegToReg>;
    table[0x03] = FINTRZ<RegToReg>;
    table[0x04] = FSQRT<RegToReg>;
    table[0x06] = FLOGNP1<RegToReg>;
    table[0x08] = FETOXM1<RegToReg>;
    table[0x09] = FTANH<RegToReg>;
    table[0x0A] = FATAN<RegToReg>;
    table[0x0C] = FASIN<RegToReg>;
    table[0x0D] = FATANH<RegToReg>;
    table[0x0E] = FSIN<RegToReg>;
    table[0x0F] = FTAN<RegToReg>;
    table[0x10] = FETOX<RegToReg>;
    table[0x11] = FTWOTOX<RegToReg>;
    table[0x12] = FTENTOX<RegToReg>;
    table[0x14] = FLOGN<RegToReg>;
    table[0x15] = FLOG10<RegToReg>;
    table[0x16] = FLOG2<RegToReg>;
    table[0x18] = FABS<RegToReg>;
    table[0x19] = FCOSH<RegToReg>;
    table[0x1A] = FNEG<RegToReg>;
    table[0x5A] = FNEG<RegToReg, DEFAULT_EA, DEFAULT_EA, true>;   // FSNEG
    table[0x5E] = FNEG<RegToReg>;   // FDNEG
    table[0x1C] = FACOS<RegToReg>;
    table[0x1D] = FCOS<RegToReg>;
    //table[0x1E] = FGETEXP<RegToReg>;
    //table[0x1F] = FGETMAN<RegToReg>;
    table[0x20] = FDIV<RegToReg>;
    table[0x60] = FDIV<RegToReg, DEFAULT_EA, DEFAULT_EA, true>;   // FSDIV
    table[0x64] = FDIV<RegToReg>;   // FDDIV
    //table[0x21] = FMOD<RegToReg>;
    table[0x22] = FADD<RegToReg>;
    table[0x62] = FADD<RegToReg, DEFAULT_EA, DEFAULT_EA, true>;   // FSADD
    table[0x66] = FADD<RegToReg>;   // FDADD
    table[0x23] = FMUL<RegToReg>;
    table[0x63] = FMUL<RegToReg, DEFAULT_EA, DEFAULT_EA, true>;   // FSMUL
    table[0x67] = FMUL<RegToReg>;   // FDMUL
    table[0x24] = FSGLDIV<RegToReg>;
    //table[0x25] = FREM<RegToReg>;
    //table[0x26] = FSCALE<RegToReg>;
    table[0x27] = FSGLMUL<RegToReg>;
    table[0x28] = FSUB<RegToReg>;
    table[0x68] = FSUB<RegToReg, DEFAULT_EA, DEFAULT_EA, true>;   // FSSUB
    table[0x6c] = FSUB<RegToReg>;   // FDSUB
    /* table[0x30..0x37] = FSINCOS */
    table[0x38] = FCMP<RegToReg>;
    table[0x3A] = FTST<RegToReg>;
    table[0x40] = FMOVE<RegToReg, DEFAULT_EA, DEFAULT_EA, true>; // FSMOVE
    table[0x44] = FMOVE<RegToReg>; // FDMOVE

    return table;
}

static constexpr auto ExtensionFieldService_RegToReg = buildExtensionFieldService<true>();
static constexpr auto ExtensionFieldService_EaToReg = buildExtensionFieldService<false>();

void handleGeneralType(uint32_t opcode)
{
    uint32_t opcode2 = *getPC<uint16_t*>(2);
    uint8_t opclass = (opcode2 >> 13) & 7;
    uint8_t rx = (opcode2 >> 10) & 7;
    uint8_t ry = (opcode2 >> 7) & 7;
    uint8_t extension = opcode2 & 0x7f;
    bool handled = false;

    advancePC(4);

    if ((opclass == 0) && ((opcode & 0x3f) == 0)) {
        handled = ExtensionFieldService_RegToReg[extension](opcode, opcode2);
    }
    else if ((opclass == 2) && (rx != 7)) {
        handled = ExtensionFieldService_EaToReg[extension](opcode, opcode2);
    }
    else if ((opclass == 2) && (rx == 7) && ((opcode & 0x3f) == 0)) {
        handled = FMOVECR(ry, extension);
    }
    else if ((opclass == 3)) {
        handled = FMOVE_to_EA(opcode, opcode2);
    }
    else if ((opclass == 4) && (ry == 0) && (extension == 0)) {
        handled = FMOVE_EA_to_Sys(opcode, opcode2);
    }
    else if ((opclass == 5) && (ry == 0) && (extension == 0)) {
        handled = FMOVE_Sys_to_EA(opcode, opcode2);
    }
    else if ((opclass == 6) && ((ry & 6) == 0)) {
        handled = FMOVEM_from_EA(opcode, opcode2);
    }
    else if ((opclass == 7) && ((ry & 6) == 0)) {
        handled = FMOVEM_to_EA(opcode, opcode2);
    }

    if (!handled) {
        ILLEGAL_LINE_F(opcode, opcode2);
    }
}

/* PFLUSH in all variants is ignored as long as there is no MMU */
void PFLUSH(uint32_t)
{
    /* PFLUSH requires supervisor rights */
    if (SR & SR_S) {
        advancePC(2);
    } else {
        raiseException(VECTOR_PRIVILEGE_VIOLATION, ExceptionFrameFormat::FORMAT_0, 0, 0);
    }
}

/* PTEST in all variants is ignored as long as there is no MMU */
void PTEST(uint32_t)
{
    /* PTEST requires supervisor rights */
    if (SR & SR_S) {
        advancePC(2);
    } else {
        raiseException(VECTOR_PRIVILEGE_VIOLATION, ExceptionFrameFormat::FORMAT_0, 0, 0);
    }
}

/* CINV - cache invalidate */
void CINV(uint32_t opcode)
{
    /* CINV requires supervisor rights */
    if (SR & SR_S) {
        const bool data_cache = (opcode & (1 << 6)) != 0;
        const bool insn_cache = (opcode & (1 << 7)) != 0;
        const int scope = (opcode >> 3) & 3;

        if (scope == 0) {
            raiseException(VECTOR_PRIVILEGE_VIOLATION, ExceptionFrameFormat::FORMAT_0, 0, 0);
        } else {
            advancePC(2);

            if (data_cache) {
                uint32_t tmp;

                switch (scope) {
                    case 1: // Line
                        tmp = getA<uint32_t>(opcode & 7);
                        tmp &= (1 << dcache_mask_bits) - 1;
                        asm volatile("dc ivac, %0; dsb sy;"::"r"(tmp));
                        break;
                    case 2: // Page
                        tmp = getA<uint32_t>(opcode & 7);
                        tmp = tmp & ~4095;
                        for (int i=0; i < (1 << (12 - dcache_mask_bits)); i++) {
                            asm volatile("dc ivac, %0"::"r"(tmp));
                            tmp += 1 << dcache_mask_bits;
                        }
                        asm volatile("dsb sy");
                        break;
                    case 3: // All
                        invalidate_entire_dcache();
                        break;
                }
            }

            if (insn_cache) {
                cache_invalidate_all(ICACHE);
                LRU_InvalidateAll();
                EPOCH++;
            }
        }
    } else {
        raiseException(VECTOR_PRIVILEGE_VIOLATION, ExceptionFrameFormat::FORMAT_0, 0, 0);
    }
}

/* CPUSH - cache flush and invalidate */
void CPUSH(uint32_t opcode)
{
    /* CPUSH requires supervisor rights */
    if (SR & SR_S) {
        const bool data_cache = (opcode & (1 << 6)) != 0;
        const bool insn_cache = (opcode & (1 << 7)) != 0;
        const int scope = (opcode >> 3) & 3;

        if (scope == 0) {
            raiseException(VECTOR_PRIVILEGE_VIOLATION, ExceptionFrameFormat::FORMAT_0, 0, 0);
        } else {
            advancePC(2);

            if (data_cache) {
                uint32_t tmp;

                switch (scope) {
                    case 1: // Line
                        tmp = getA<uint32_t>(opcode & 7);
                        tmp &= (1 << dcache_mask_bits) - 1;
                        asm volatile("dc civac, %0; dsb sy;"::"r"(tmp));
                        break;
                    case 2: // Page
                        tmp = getA<uint32_t>(opcode & 7);
                        tmp = tmp & ~4095;
                        for (int i=0; i < (1 << (12 - dcache_mask_bits)); i++) {
                            asm volatile("dc civac, %0"::"r"(tmp));
                            tmp += 1 << dcache_mask_bits;
                        }
                        asm volatile("dsb sy");
                        break;
                    case 3: // All
                        clear_entire_dcache();
                        break;
                }
            }

            if (insn_cache) {
                cache_invalidate_all(ICACHE);
                LRU_InvalidateAll();
                EPOCH++;
            }
        }
    } else {
        raiseException(VECTOR_PRIVILEGE_VIOLATION, ExceptionFrameFormat::FORMAT_0, 0, 0);
    }
}

template<bool RegToReg, uint8_t Reg, uint8_t Mode>
void MOVE16(uint32_t)
{
    if constexpr (RegToReg) {
        uint32_t regAy = (*getPC<uint16_t*>(2) >> 12) & 7;
        uint32_t regAyAddr = getA<uint32_t>(regAy);
        uint128_t* src;
        uint128_t* dst;

        advancePC(4);
        commitPC();

        dst = (uint128_t*)((uintptr_t)regAyAddr & ~15);
        src = (uint128_t*)((uintptr_t)getA<Reg, uint32_t>() & ~15);
        
        *dst = *src;

        setA<Reg, uint32_t>(getA<Reg, uint32_t>() + 16);
        setA<uint32_t>(regAy, regAyAddr + 16);
    } else {
        uintptr_t mem = *getPC<uint32_t*>(2) & ~15;

        advancePC(6);
        commitPC();

        uint128_t* memaddr = (uint128_t*)mem;
        uint128_t* regaddr = (uint128_t*)((uintptr_t)getA<Reg, uint32_t>() & ~15);

        /* If bit 0 of opmode is set, direction is memory addr to reg addr */
        if constexpr ((Mode & 1) == 1) {
            *regaddr = *memaddr;
        } else {
            *memaddr = *regaddr;
        }

        /* If bit 1 of opmode is clear, update Reg */
        if constexpr ((Mode & 2) == 0) {
            setA<Reg, uint32_t>(getA<Reg, uint32_t>() + 16);
        }
    }
}

#define FILL_MOD(base_offset, mod, rmin, rmax, name, specialized) \
    [&]<std::size_t... Dreg>(int base, std::index_sequence<Dreg...>) { \
        if constexpr (specialized) ((table[base + EA(mod, (rmin + Dreg))] = \
            name<mod, (rmin + Dreg)>), ...); \
        else ((table[base + EA(mod, (rmin) + Dreg)] = \
            name<DEFAULT_EA, DEFAULT_EA>), ...); \
    }((base_offset), std::make_index_sequence<(rmax - rmin + 1)>{});

#define FILL_Bcc(long) \
    [&]<std::size_t... cc>(std::index_sequence<cc...>) { \
        if constexpr(long) ((table[cc + 01300] = \
            FBcc<true, cc>), ...); \
        else ((table[cc + 01200] = \
            FBcc<false, cc>), ...); \
    }(std::make_index_sequence<32>{});
#if 0
static consteval std::array<INTERPRET_Function, 512> buildFPUTable()
{
    std::array<INTERPRET_Function, 512> table{};

    auto fill = [&table](int first, int last, INTERPRET_Function func) {
        for (int i = first; i <= last; ++i) {
            table[i] = func;
        }
    };

    auto EA = [](int mode, int reg) constexpr { return (mode << 3) | reg; };

    /* Make all entries unimplemented first */
    fill(00000, 0777, ILLEGAL);

    /* Type 0b000: General FPU instructions */
    fill(0x000, 0077, handleGeneralType);

    /* Type 0b001: FDBcc, FScc, FTRAPcc */

    /* Type 0b010: FBcc.W */
    FILL_Bcc(false);

    /* Type 0b011: FBcc.L */
    FILL_Bcc(true);

    /* Type 0b100: FSAVE, specialized and generic version */
    FILL_MOD(0400, 2, 0, 7, FSAVE, true);
    FILL_MOD(0400, 4, 0, 7, FSAVE, true);
    FILL_MOD(0400, 5, 0, 7, FSAVE, false);
    FILL_MOD(0400, 6, 0, 7, FSAVE, false);
    FILL_MOD(0400, 7, 0, 1, FSAVE, false);

    /* Type 0b101:FRESTORE, specialized and generic version */
    FILL_MOD(0500, 2, 0, 7, FRESTORE, true);
    FILL_MOD(0500, 3, 0, 7, FRESTORE, true);
    FILL_MOD(0500, 5, 0, 7, FRESTORE, false);
    FILL_MOD(0500, 6, 0, 7, FRESTORE, false);
    FILL_MOD(0500, 7, 0, 3, FRESTORE, false);

    return table;
}
#endif


template <uint8_t Reg, uint8_t Mode>
struct MOVE16_RegMem_Op { static constexpr auto value = MOVE16<false, Reg, Mode>; };

template <uint8_t Reg>
struct MOVE16_RegReg_Op { static constexpr auto value = MOVE16<true, Reg, 0>; };

template <template<uint8_t,uint8_t> class Op, class F1, class F2>
constexpr void fillRegReg(std::array<INTERPRET_Function, 4096>& table, int base)
{
    auto fillOne = [&]<std::size_t I>() {
        constexpr unsigned v1 = I / F2::size;
        constexpr unsigned v2 = I % F2::size;
        if constexpr (F1::valid(v1) && F2::valid(v2)) {
            int idx = base + (v1 << F1::bitOffset) + (v2 << F2::bitOffset);
            table[idx] = Op<F1::arg(v1), F2::arg(v2)>::value;
        }
    };
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        (fillOne.template operator()<Is>(), ...);
    }(std::make_index_sequence<F1::size * F2::size>{});
}

template <template<uint8_t> class Op, class F>
constexpr void fillReg(std::array<INTERPRET_Function, 4096>& table, int base)
{
    auto fillOne = [&]<std::size_t I>() {
        if constexpr (F::valid(I)) {
            int idx = base + (I << F::bitOffset);
            table[idx] = Op<F::arg(I)>::value;
        }
    };
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        (fillOne.template operator()<Is>(), ...);
    }(std::make_index_sequence<F::size>{});
}

static constexpr std::array<INTERPRET_Function, 4096> buildInsnTable()
{
    std::array<INTERPRET_Function, 4096> table{};
    for (auto& e : table) e = ILLEGAL_LINE_F;

    auto EA = [](int mode, int reg) constexpr { return (mode << 3) | reg; };
    auto fill = [&table](int first, int last, INTERPRET_Function func) {
        for (int i = first; i <= last; ++i) {
            table[i] = func;
        }
    };

    /* Type 0b000: General FPU instructions */
    fill(01000, 01077, handleGeneralType);

    /* Type 0b001: FDBcc, FScc, FTRAPcc */

    /* Type 0b010: FBcc.W */
    FILL_Bcc(false);

    /* Type 0b011: FBcc.L */
    FILL_Bcc(true);

    /* Type 0b100: FSAVE, specialized and generic version */
    FILL_MOD(01400, 2, 0, 7, FSAVE, true);
    FILL_MOD(01400, 4, 0, 7, FSAVE, true);
    FILL_MOD(01400, 5, 0, 7, FSAVE, false);
    FILL_MOD(01400, 6, 0, 7, FSAVE, false);
    FILL_MOD(01400, 7, 0, 1, FSAVE, false);

    /* Type 0b101:FRESTORE, specialized and generic version */
    FILL_MOD(01500, 2, 0, 7, FRESTORE, true);
    FILL_MOD(01500, 3, 0, 7, FRESTORE, true);
    FILL_MOD(01500, 5, 0, 7, FRESTORE, false);
    FILL_MOD(01500, 6, 0, 7, FRESTORE, false);
    FILL_MOD(01500, 7, 0, 3, FRESTORE, false);

    /* PFLUSH/PFLUSHA */
    fill(02400, 02437, PFLUSH);

    /* PTEST */
    fill(02510, 02517, PTEST);
    fill(02550, 02557, PTEST);

    /* CINV */
    fill(02010, 02037, CINV);
    fill(02110, 02137, CINV);
    fill(02210, 02237, CINV);
    fill(02310, 02337, CINV);

    /* CPUSH */
    fill(02050, 02077, CPUSH);
    fill(02150, 02177, CPUSH);
    fill(02250, 02277, CPUSH);
    fill(02350, 02377, CPUSH);

    fillReg<MOVE16_RegReg_Op, RegField<0, 3>>(table, 03040);
    fillRegReg<MOVE16_RegMem_Op, RegField<0, 3>, RegField<3, 2>>(table, 03000);

    return table;
}

constexpr auto InsnTable __attribute__((used,aligned(4096),section(".int.jumptable.f"))) = buildInsnTable();

} // Emu68::M68k::Interpreter::LineF
