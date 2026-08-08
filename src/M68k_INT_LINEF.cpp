#include <cstdint>
#include <arm_neon.h>
#include <array>
#include <limits>

#include <emu68/m68k/interpreter.hpp>

#include "RegisterMapping.h"

extern "C" {

#include "math/libm.h"

uint64_t Load96bit(uintptr_t __ignore, uintptr_t base);
uint64_t Store96bit(uint64_t value, uintptr_t base);

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

namespace Emu68::M68k::Interpreter {

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

[[gnu::always_inline]] inline double roundToSingle(double value)
{
    float f;
    asm volatile("fcvt %s0, %d1" : "=w"(f) : "w"(value));
    double result;
    asm volatile("fcvt %d0, %s1" : "=w"(result) : "w"(f));
    return result;
}

template<uint8_t Mode = DEFAULT, uint8_t Reg = DEFAULT>
void FSAVE(uint32_t opcode)
{
    uint8_t mode = (Mode == DEFAULT) ? ((opcode >> 3) & 7) : Mode;
    uint8_t reg = (Reg == DEFAULT) ? (opcode & 7) : Reg;

    PC += 2;

    /* Save only IDLE frame, we not need to record any changes here */
    if constexpr (Mode == DEFAULT || Reg == DEFAULT) { 
        storeToEffectiveAddress(reg, 0x41000000, 4, mode);
    } else {
        storeToEA<Mode, Reg, uint32_t>(0x41000000);
    }
}

template<uint8_t Mode = DEFAULT, uint8_t Reg = DEFAULT>
void FRESTORE(uint32_t opcode)
{
    uint32_t state;
    uint8_t mode = (Mode == DEFAULT) ? ((opcode >> 3) & 7) : Mode;
    uint8_t reg = (Reg == DEFAULT) ? (opcode & 7) : Reg;

    PC += 2;

    /* Restore frame header */
    if constexpr (Mode == DEFAULT || Reg == DEFAULT) {
        loadFromEffectiveAddress(reg, 4, &state, mode);
    } else { 
        state = loadFromEA<Mode, Reg, uint32_t>();
    }

    /* If restored NULL frame, reset FPU state */
    if ((state & 0xff000000) == 0) {
        resetFPUState();
    }
}

bool ILLEGAL(uint32_t opcode, uint32_t)
{
    PC -= 4;

    ILLEGAL(opcode);

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
            case 0: out = (double)getDn<int32_t>(reg); return true;
            case 1: u32.u32 = getDn<int32_t>(reg); out = u32.f; return true;
            case 4: out = (double)getDn<int16_t>(reg); return true;
            case 6: out = (double)getDn<int8_t>(reg); return true;
        }
    }

    /* Anything else with mode 0 or mode 1 is a reserved/illegal encoding. */
    if (mode < 2) {
        return false;
    }

    getEffectiveAddress(reg, size, &addr, mode);

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
    uint32_t pc = PC + 2;
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

    PC = pc;
}

template<bool RegToReg, uint8_t DstReg, uint8_t SrcReg, bool UpdateDst = true, class Fn>
bool MONADIC(uint32_t opcode, uint32_t opcode2, Fn&& modify)
{
    uint8_t src = (SrcReg == DEFAULT) ? (opcode2 >> 10) & 7 : SrcReg;
    uint8_t dst = (DstReg == DEFAULT) ? (opcode2 >> 7) & 7 : DstReg;
    double value;

    if constexpr (RegToReg) {
        if constexpr (DstReg == DEFAULT) {
            value = modify(getFPn<double>(src));
            if constexpr (UpdateDst) { setFPn<double>(dst, value); }
        } else if constexpr (SrcReg == DEFAULT) {
            value = modify(getFPn<double>(src));
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

        if constexpr (DstReg == DEFAULT) {
            if constexpr (UpdateDst) setFPn<double>(dst, value);
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
    uint8_t src = (SrcReg == DEFAULT) ? (opcode2 >> 10) & 7 : SrcReg;
    uint8_t dst = (DstReg == DEFAULT) ? (opcode2 >> 7) & 7 : DstReg;
    double value;

    if constexpr (RegToReg) {
        if constexpr (DstReg == DEFAULT) {
            value = modify(getFPn<double>(src), getFPn<double>(dst));
            if constexpr (UpdateDst) { setFPn<double>(dst, value); }
        } else if constexpr (SrcReg == DEFAULT) {
            value = modify(getFPn<double>(src), getFP<DstReg, double>());
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

        if constexpr (DstReg == DEFAULT) {
            value = modify(value, getFPn<double>(dst));
            if constexpr (UpdateDst) { setFPn<double>(dst, value); }
        } else {
            value = modify(value, getFP<DstReg, double>());
            if constexpr (UpdateDst) { setFP<DstReg, double>(value); }
        }
    }

    updateFlagsFPU(value);

    return true;
}

/* Dyadic operations */

template<bool RegToReg, uint8_t DstReg = DEFAULT, uint8_t SrcReg = DEFAULT, bool SingleRounding = false>
bool FADD(uint32_t opcode, uint32_t opcode2)
{
    return DYADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double op1, double op2) -> double { 
        double result = op1 + op2; 
        if constexpr (SingleRounding) { result = roundToSingle(result); }
        return result;
    });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT, uint8_t SrcReg = DEFAULT>
bool FCMP(uint32_t opcode, uint32_t opcode2)
{
    return DYADIC<RegToReg, DstReg, SrcReg, false>(opcode, opcode2, [](double op1, double op2) -> double { return op2 - op1; });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT, uint8_t SrcReg = DEFAULT, bool SingleRounding = false>
bool FDIV(uint32_t opcode, uint32_t opcode2)
{
    return DYADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double op1, double op2) -> double { 
        double result = op2 / op1; 
        if constexpr (SingleRounding) { result = roundToSingle(result); }
        return result;
    });
}

// FMOD

template<bool RegToReg, uint8_t DstReg = DEFAULT, uint8_t SrcReg = DEFAULT, bool SingleRounding = false>
bool FMUL(uint32_t opcode, uint32_t opcode2)
{
    return DYADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double op1, double op2) -> double { 
        double result = op2 * op1; 
        if constexpr (SingleRounding) { result = roundToSingle(result); }
        return result;
    });
}

// FREM

// FSCALE

// FSGLDIV

// FSGLMUL

template<bool RegToReg, uint8_t DstReg = DEFAULT, uint8_t SrcReg = DEFAULT, bool SingleRounding = false>
bool FSUB(uint32_t opcode, uint32_t opcode2)
{
    return DYADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double op1, double op2) -> double { 
        double result = op2 - op1; 
        if constexpr (SingleRounding) { result = roundToSingle(result); }
        return result;
    });
}

/* Monadic operations */

template<bool RegToReg, uint8_t DstReg = DEFAULT, uint8_t SrcReg = DEFAULT>
bool FABS(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { return fabs(value); });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT, uint8_t SrcReg = DEFAULT>
bool FACOS(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { return acos(value); });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT, uint8_t SrcReg = DEFAULT>
bool FASIN(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { return asin(value); });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT, uint8_t SrcReg = DEFAULT>
bool FATAN(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { return atan(value); });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT, uint8_t SrcReg = DEFAULT>
bool FATANH(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { return atanh(value); });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT, uint8_t SrcReg = DEFAULT>
bool FCOS(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { return cos(value); });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT, uint8_t SrcReg = DEFAULT>
bool FCOSH(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { return cosh(value); });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT, uint8_t SrcReg = DEFAULT>
bool FETOX(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { return exp(value); });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT, uint8_t SrcReg = DEFAULT>
bool FETOXM1(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { return expm1(value); });
}

// FGETEXP

// FGETMAN

template<bool RegToReg, uint8_t DstReg = DEFAULT, uint8_t SrcReg = DEFAULT>
bool FINT(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { asm volatile("frinti %d0, %d0" : "=w"(value) : "0"(value)); return value; });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT, uint8_t SrcReg = DEFAULT>
bool FINTRZ(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { asm volatile("frintz %d0, %d0" : "=w"(value) : "0"(value)); return value; });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT, uint8_t SrcReg = DEFAULT>
bool FLOG10(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { return log10(value); });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT, uint8_t SrcReg = DEFAULT>
bool FLOG2(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { return log2(value); });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT, uint8_t SrcReg = DEFAULT>
bool FLOGN(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { return log(value); });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT, uint8_t SrcReg = DEFAULT>
bool FLOGNP1(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { return log1p(value); });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT, uint8_t SrcReg = DEFAULT, bool SingleRounding = false>
bool FMOVE(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { 
        if constexpr (SingleRounding) { return roundToSingle(value); }
        else { return value; }
    });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT, uint8_t SrcReg = DEFAULT, bool SingleRounding = false>
bool FNEG(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { 
        if constexpr (SingleRounding) { return roundToSingle(-value); }
        else { return -value; }
    });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT, uint8_t SrcReg = DEFAULT>
bool FSIN(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { return sin(value); });
}

// FSINCOS - special case!

template<bool RegToReg, uint8_t DstReg = DEFAULT, uint8_t SrcReg = DEFAULT>
bool FSINH(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { return sinh(value); });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT, uint8_t SrcReg = DEFAULT>
bool FSQRT(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { return sqrt(value); });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT, uint8_t SrcReg = DEFAULT>
bool FTAN(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { return tan(value); });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT, uint8_t SrcReg = DEFAULT>
bool FTANH(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { return tanh(value); });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT, uint8_t SrcReg = DEFAULT>
bool FTENTOX(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { return exp10(value); });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT, uint8_t SrcReg = DEFAULT>
bool FTST(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg, false>(opcode, opcode2, [](double value) -> double { return value; });
}

template<bool RegToReg, uint8_t DstReg = DEFAULT, uint8_t SrcReg = DEFAULT>
bool FTWOTOX(uint32_t opcode, uint32_t opcode2)
{
    return MONADIC<RegToReg, DstReg, SrcReg>(opcode, opcode2, [](double value) -> double { return exp2(value); });
}

bool FMOVECR(uint8_t ry, uint8_t extension)
{
    setFPn<double>(ry, FPUConstants[extension]);

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
        reglist = getDn<uint8_t>((reglist >> 4) & 7);
    }
    
    /* Predecrement mode is different as we need to update the address */
    if (mode == 4) {
        addr = getAn<uint32_t>(reg);

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

        setAn<uint32_t>(reg, addr);
    } else {
        getEffectiveAddress(reg, 0, &addr, mode);
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
        reglist = getDn<uint8_t>((reglist >> 4) & 7);
    }
    
    getEffectiveAddress(reg, 0, &addr, mode);

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
        setAn<uint32_t>(reg, addr);
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
            case 4: setDn<uint32_t>(reg, FPCR); break;
            case 2: setDn<uint32_t>(reg, FPSR); break;
            case 1: setDn<uint32_t>(reg, FPIAR); break;
            default: handled = false;
        }
    } else if (mode == 1) {
        if (reglist == 1) setAn<uint32_t>(reg, FPIAR);
        else handled = false;
    } else {
        uint32_t size = 0;
        uint32_t addr = 0;

        if (reglist & 1) size += 4;
        if (reglist & 2) size += 4;
        if (reglist & 4) size += 4;

        getEffectiveAddress(reg, size, &addr, mode);

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
            case 4: FPCR = getDn<uint32_t>(reg); break;
            case 2: FPSR = getDn<uint32_t>(reg); break;
            case 1: FPIAR = getDn<uint32_t>(reg); break;
            default: handled = false;
        }
    } else if (mode == 1) {
        if (reglist == 1) FPIAR = getAn<uint32_t>(reg);
        else handled = false;
    } else {
        uint32_t size = 0;
        uint32_t addr = 0;

        if (reglist & 1) size += 4;
        if (reglist & 2) size += 4;
        if (reglist & 4) size += 4;

        getEffectiveAddress(reg, size, &addr, mode);

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
    uint32_t addr;
    int size;
    int k;
    union {
        uint64_t u64;
        double d;
    } f64;
    union {
        uint32_t u32;
        float f;
    } f32;

    double value = getFPn<double>((opcode2 >> 7) & 7);

    if (mode == 1) { return false; }

    if (mode == 0 && (format == 0 || format == 1 || format == 4 || format == 6)) {
        switch (format) {
            case 0: setDn<int32_t>(reg, (int32_t)value); break;
            case 1: f32.f = value; setDn<uint32_t>(reg, f32.u32); break;
            case 4: setDn<int16_t>(reg, (int16_t)value); break;
            case 6: setDn<int8_t>(reg, (int8_t)value); break;
        }
        return true;
    } else {
        if (mode < 2) { return false; }

        switch (format) {
            case 0: size = 4; break;
            case 1: size = 4; break;
            case 2: size = 12; break;
            case 3: size = 12; k = opcode2 & 0x7f; break;
            case 4: size = 2; break;
            case 5: size = 8; break;
            case 6: size = 1; break;
            case 7: size = 12; k = getDn<uint8_t>((opcode2 >> 4) & 7); break;
        }

        getEffectiveAddress(reg, size, &addr, mode);

        switch (format) {
            case 0: *(int32_t*)(uintptr_t)addr = (int32_t)value; break;
            case 1: *(float*)(uintptr_t)addr = (float)value; break;
            case 2: f64.d = value; Store96bit(f64.u64, addr); break;
            case 3: *(packed_t*)(uintptr_t)addr = DoubleToPacked(value, k); break;
            case 4: *(uint16_t*)(uintptr_t)addr = (int16_t)value; break;
            case 5: *(double*)(uintptr_t)addr = value; break;
            case 6: *(int8_t*)(uintptr_t)addr = (int8_t)value; break;
            case 7: *(packed_t*)(uintptr_t)addr = DoubleToPacked(value, k); break;
        }
    }

    return true;
}

template<bool RegToReg>
static consteval std::array<INTERPRET_FPU_Function, 128> buildExtensionFieldService()
{
    std::array<INTERPRET_FPU_Function, 128> table{};

    for (int i = 0; i < 128; ++i) {
        table[i] = ILLEGAL;
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
    table[0x5A] = FNEG<RegToReg, DEFAULT, DEFAULT, true>;   // FSNEG
    table[0x5E] = FNEG<RegToReg>;   // FDNEG
    table[0x1C] = FACOS<RegToReg>;
    table[0x1D] = FCOS<RegToReg>;
    //table[0x1E] = FGETEXP<RegToReg>;
    //table[0x1F] = FGETMAN<RegToReg>;
    table[0x20] = FDIV<RegToReg>;
    table[0x60] = FDIV<RegToReg, DEFAULT, DEFAULT, true>;   // FSDIV
    table[0x64] = FDIV<RegToReg>;   // FDDIV
    //table[0x21] = FMOD<RegToReg>;
    table[0x22] = FADD<RegToReg>;
    table[0x62] = FADD<RegToReg, DEFAULT, DEFAULT, true>;   // FSADD
    table[0x66] = FADD<RegToReg>;   // FDADD
    table[0x23] = FMUL<RegToReg>;
    table[0x63] = FMUL<RegToReg, DEFAULT, DEFAULT, true>;   // FSMUL
    table[0x67] = FMUL<RegToReg>;   // FDMUL
    //table[0x24] = FSGLDIV<RegToReg>;
    //table[0x25] = FREM<RegToReg>;
    //table[0x26] = FSCALE<RegToReg>;
    //table[0x27] = FSGLMUL<RegToReg>;
    table[0x28] = FSUB<RegToReg>;
    table[0x68] = FSUB<RegToReg, DEFAULT, DEFAULT, true>;   // FSSUB
    table[0x6c] = FSUB<RegToReg>;   // FDSUB
    /* table[0x30..0x37] = FSINCOS */
    table[0x38] = FCMP<RegToReg>;
    table[0x3A] = FTST<RegToReg>;
    table[0x40] = FMOVE<RegToReg, DEFAULT, DEFAULT, true>; // FSMOVE
    table[0x44] = FMOVE<RegToReg>; // FDMOVE

    return table;
}

static constexpr auto ExtensionFieldService_RegToReg = buildExtensionFieldService<true>();
static constexpr auto ExtensionFieldService_EaToReg = buildExtensionFieldService<false>();

void handleGeneralType(uint32_t opcode)
{
    uint32_t opcode2 = *(uint16_t *)(uintptr_t)(PC + 2);
    uint8_t opclass = (opcode2 >> 13) & 7;
    uint8_t rx = (opcode2 >> 10) & 7;
    uint8_t ry = (opcode2 >> 7) & 7;
    uint8_t extension = opcode2 & 0x7f;
    bool handled = false;

    PC += 4;

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
        ILLEGAL(opcode, opcode2);
    }
}

#define FILL_MOD(base_offset, mod, rmin, rmax, name, specialized) \
    [&]<std::size_t... Dreg>(int base, std::index_sequence<Dreg...>) { \
        if constexpr (specialized) ((table[base + EA(mod, (rmin + Dreg))] = \
            name<mod, (rmin + Dreg)>), ...); \
        else ((table[base + EA(mod, (rmin) + Dreg)] = \
            name<DEFAULT, DEFAULT>), ...); \
    }((base_offset), std::make_index_sequence<(rmax - rmin + 1)>{});

#define FILL_Bcc(long) \
    [&]<std::size_t... cc>(std::index_sequence<cc...>) { \
        if constexpr(long) ((table[cc + 0300] = \
            FBcc<true, cc>), ...); \
        else ((table[cc + 0200] = \
            FBcc<false, cc>), ...); \
    }(std::make_index_sequence<32>{});

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

} // Emu68::M68k::Interpreter

static constexpr auto InsnTable_FPU = Emu68::M68k::Interpreter::buildFPUTable();

__attribute__((optimize("no-optimize-sibling-calls")))
void INTERPRET_lineF(uint32_t opcode)
{
    switch ((opcode >> 9) & 7)
    {
        case 1:     InsnTable_FPU[opcode & 0x1ff](opcode); break;
        default:    Emu68::M68k::Interpreter::ILLEGAL(opcode);
    }
}
