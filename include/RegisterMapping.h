#ifndef _REGISTERMAPPING_H
#define _REGISTERMAPPING_H

#include <stdint.h>
#include "M68k.h"

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

register double FP0 asm("d8");
register double FP1 asm("d9");
register double FP2 asm("d10");
register double FP3 asm("d11");
register double FP4 asm("d12");
register double FP5 asm("d13");
register double FP6 asm("d14");
register double FP7 asm("d15");

[[gnu::always_inline]] inline uint32_t getPC()
{
    return PC;
}

[[gnu::always_inline]] inline void setPC(uint32_t pc)
{
    PC = pc;
    //asm volatile("" : "+r"(PC));
}

[[gnu::always_inline]] inline void advancePC(int delta)
{
    PC += delta;
    //asm volatile("" : "+r"(PC));
}

[[gnu::always_inline]] inline void commitPC()
{
    asm volatile("" : "+r"(PC));
}

/* 
    Even though SR is 16 bit, we return and set it as a 32 bit value 
    to make compiler happier
*/
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

static inline uint32_t getCACR()
{
    uint32_t cacr;
    __asm__ volatile("umov %w0, " REG_CACR_ASM:"=r"(cacr));
    return cacr;
}

static inline void setCACR(uint32_t cacr)
{
    __asm__ volatile("mov " REG_CACR_ASM ", %w0": :"r"(cacr));
}

static inline uint32_t getUSP()
{
    uint32_t usp;
    __asm__ volatile("umov %w0, " REG_USP_ASM:"=r"(usp));
    return usp;
}

static inline void setUSP(uint32_t usp)
{
    __asm__ volatile("mov " REG_USP_ASM ", %w0": :"r"(usp));
}

static inline uint32_t getMSP()
{
    uint32_t msp;
    __asm__ volatile("umov %w0, " REG_MSP_ASM:"=r"(msp));
    return msp;
}

static inline void setMSP(uint32_t msp)
{
    __asm__ volatile("mov " REG_MSP_ASM ", %w0": :"r"(msp));
}

static inline uint32_t getISP()
{
    uint32_t isp;
    __asm__ volatile("umov %w0, " REG_ISP_ASM:"=r"(isp));
    return isp;
}

static inline void setISP(uint32_t isp)
{
    __asm__ volatile("mov " REG_ISP_ASM ", %w0": :"r"(isp));
}

static inline uint32_t getFPIAR()
{
    uint32_t fpiar;
    __asm__ volatile("umov %w0, " REG_FPIAR_ASM :"=r"(fpiar));
    return fpiar;
}

static inline void setFPIAR(uint32_t fpiar)
{
    __asm__ volatile("mov " REG_FPIAR_ASM ", %w0": :"r"(fpiar));
}

static inline uint32_t getFPSR()
{
    uint32_t fpsr;
    __asm__ volatile("umov %w0, " REG_FPSR_ASM :"=r"(fpsr));
    return fpsr;
}

static inline void setFPSR(uint32_t fpsr)
{
    __asm__ volatile("mov " REG_FPSR_ASM ", %w0": :"r"(fpsr));
}

static inline uint32_t getFPCR()
{
    uint32_t fpcr;
    __asm__ volatile("umov %w0, " REG_FPCR_ASM :"=r"(fpcr));
    return fpcr;
}

static inline void setFPCR(uint32_t fpcr)
{
    __asm__ volatile("mov " REG_FPCR_ASM ", %w0": :"r"(fpcr));
}

#ifdef __cplusplus

#include <type_traits> 

template<class Type>
requires (std::is_pointer<Type>::value || std::is_same<Type, uint32_t>::value)
Type getPC(int offset = 0)
{
    return (Type)(uintptr_t)(PC + offset);
}

template<class Type>
Type getD(int reg);

template<class Type>
void setD(int reg, Type val);

template<class Type>
Type getA(int reg);

template<class Type>
void setA(int reg, Type val);

template<class Type>
Type getFP(int reg);

template<class Type>
void setFP(int reg, Type val);

template <unsigned reg, class type> requires (reg < 8)
[[gnu::always_inline]] inline type getD()
{
    if constexpr (reg == 0)      { return (type)D0; }
    else if constexpr (reg == 1) { return (type)D1; }
    else if constexpr (reg == 2) { return (type)D2; }
    else if constexpr (reg == 3) { return (type)D3; }
    else if constexpr (reg == 4) { return (type)D4; }
    else if constexpr (reg == 5) { return (type)D5; }
    else if constexpr (reg == 6) { return (type)D6; }
    else if constexpr (reg == 7) { return (type)D7; }
}

template <unsigned reg, class type> requires (reg < 8 && std::is_integral<type>::value && (sizeof(type) == 1 || sizeof(type) == 2 || sizeof(type) == 4))
[[gnu::always_inline]] inline void setD(type value)
{
    if constexpr (sizeof(type) == 4) {
        if constexpr (reg == 0)      { D0 = value; asm volatile("" : "+r"(D0)); }
        else if constexpr (reg == 1) { D1 = value; asm volatile("" : "+r"(D1)); }
        else if constexpr (reg == 2) { D2 = value; asm volatile("" : "+r"(D2)); }
        else if constexpr (reg == 3) { D3 = value; asm volatile("" : "+r"(D3)); }
        else if constexpr (reg == 4) { D4 = value; asm volatile("" : "+r"(D4)); }
        else if constexpr (reg == 5) { D5 = value; asm volatile("" : "+r"(D5)); }
        else if constexpr (reg == 6) { D6 = value; asm volatile("" : "+r"(D6)); }
        else if constexpr (reg == 7) { D7 = value; asm volatile("" : "+r"(D7)); }
    }
    else if constexpr (sizeof(type) == 2) {
        if constexpr (reg == 0)      { D0 = (D0 & 0xffff0000) | (value & 0x0000ffff); asm volatile("" : "+r"(D0)); }
        else if constexpr (reg == 1) { D1 = (D1 & 0xffff0000) | (value & 0x0000ffff); asm volatile("" : "+r"(D1)); }
        else if constexpr (reg == 2) { D2 = (D2 & 0xffff0000) | (value & 0x0000ffff); asm volatile("" : "+r"(D2)); }
        else if constexpr (reg == 3) { D3 = (D3 & 0xffff0000) | (value & 0x0000ffff); asm volatile("" : "+r"(D3)); }
        else if constexpr (reg == 4) { D4 = (D4 & 0xffff0000) | (value & 0x0000ffff); asm volatile("" : "+r"(D4)); }
        else if constexpr (reg == 5) { D5 = (D5 & 0xffff0000) | (value & 0x0000ffff); asm volatile("" : "+r"(D5)); }
        else if constexpr (reg == 6) { D6 = (D6 & 0xffff0000) | (value & 0x0000ffff); asm volatile("" : "+r"(D6)); }
        else if constexpr (reg == 7) { D7 = (D7 & 0xffff0000) | (value & 0x0000ffff); asm volatile("" : "+r"(D7)); }
    }
    else if constexpr (sizeof(type) == 1) {
        if constexpr (reg == 0)      { D0 = (D0 & 0xffffff00) | (value & 0x000000ff); asm volatile("" : "+r"(D0)); }
        else if constexpr (reg == 1) { D1 = (D1 & 0xffffff00) | (value & 0x000000ff); asm volatile("" : "+r"(D1)); }
        else if constexpr (reg == 2) { D2 = (D2 & 0xffffff00) | (value & 0x000000ff); asm volatile("" : "+r"(D2)); }
        else if constexpr (reg == 3) { D3 = (D3 & 0xffffff00) | (value & 0x000000ff); asm volatile("" : "+r"(D3)); }
        else if constexpr (reg == 4) { D4 = (D4 & 0xffffff00) | (value & 0x000000ff); asm volatile("" : "+r"(D4)); }
        else if constexpr (reg == 5) { D5 = (D5 & 0xffffff00) | (value & 0x000000ff); asm volatile("" : "+r"(D5)); }
        else if constexpr (reg == 6) { D6 = (D6 & 0xffffff00) | (value & 0x000000ff); asm volatile("" : "+r"(D6)); }
        else if constexpr (reg == 7) { D7 = (D7 & 0xffffff00) | (value & 0x000000ff); asm volatile("" : "+r"(D7)); }
    }
}

template <unsigned reg, class type> requires (reg < 8 && sizeof(type) > 1)
[[gnu::always_inline]] inline type getA()
{
    if constexpr (reg == 0)      { return (type)A0; }
    else if constexpr (reg == 1) { return (type)A1; }
    else if constexpr (reg == 2) { return (type)A2; }
    else if constexpr (reg == 3) { return (type)A3; }
    else if constexpr (reg == 4) { return (type)A4; }
    else if constexpr (reg == 5) { return (type)A5; }
    else if constexpr (reg == 6) { return (type)A6; }
    else if constexpr (reg == 7) { return (type)A7; }
}

template <unsigned reg, class type> requires (reg < 8 && std::is_integral<type>::value && (sizeof(type) == 2 || sizeof(type) == 4))
[[gnu::always_inline]] inline void setA(type value)
{
    if constexpr (sizeof(type) == 4) {
        if constexpr (reg == 0)      { A0 = value; asm volatile("" : "+r"(A0)); }
        else if constexpr (reg == 1) { A1 = value; asm volatile("" : "+r"(A1)); }
        else if constexpr (reg == 2) { A2 = value; asm volatile("" : "+r"(A2)); }
        else if constexpr (reg == 3) { A3 = value; asm volatile("" : "+r"(A3)); }
        else if constexpr (reg == 4) { A4 = value; asm volatile("" : "+r"(A4)); }
        else if constexpr (reg == 5) { A5 = value; asm volatile("" : "+r"(A5)); }
        else if constexpr (reg == 6) { A6 = value; asm volatile("" : "+r"(A6)); }
        else if constexpr (reg == 7) { A7 = value; asm volatile("" : "+r"(A7)); }
    }
    else if constexpr (sizeof(type) == 2) {
        if constexpr (reg == 0)      { A0 = (int16_t)value; asm volatile("" : "+r"(A0)); }
        else if constexpr (reg == 1) { A1 = (int16_t)value; asm volatile("" : "+r"(A1)); }
        else if constexpr (reg == 2) { A2 = (int16_t)value; asm volatile("" : "+r"(A2)); }
        else if constexpr (reg == 3) { A3 = (int16_t)value; asm volatile("" : "+r"(A3)); }
        else if constexpr (reg == 4) { A4 = (int16_t)value; asm volatile("" : "+r"(A4)); }
        else if constexpr (reg == 5) { A5 = (int16_t)value; asm volatile("" : "+r"(A5)); }
        else if constexpr (reg == 6) { A6 = (int16_t)value; asm volatile("" : "+r"(A6)); }
        else if constexpr (reg == 7) { A7 = (int16_t)value; asm volatile("" : "+r"(A7)); }
    }
}

template <unsigned reg, class type> requires (reg < 8)
[[gnu::always_inline]] inline type getFP()
{
    if constexpr (reg == 0)      { return (type)FP0; }
    else if constexpr (reg == 1) { return (type)FP1; }
    else if constexpr (reg == 2) { return (type)FP2; }
    else if constexpr (reg == 3) { return (type)FP3; }
    else if constexpr (reg == 4) { return (type)FP4; }
    else if constexpr (reg == 5) { return (type)FP5; }
    else if constexpr (reg == 6) { return (type)FP6; }
    else if constexpr (reg == 7) { return (type)FP7; }
}

template <unsigned reg, class type> requires (reg < 8)
[[gnu::always_inline]] inline void setFP(type value)
{
    if constexpr (reg == 0)      { FP0 = value; asm volatile("" : "+r"(FP0)); }
    else if constexpr (reg == 1) { FP1 = value; asm volatile("" : "+r"(FP1)); }
    else if constexpr (reg == 2) { FP2 = value; asm volatile("" : "+r"(FP2)); }
    else if constexpr (reg == 3) { FP3 = value; asm volatile("" : "+r"(FP3)); }
    else if constexpr (reg == 4) { FP4 = value; asm volatile("" : "+r"(FP4)); }
    else if constexpr (reg == 5) { FP5 = value; asm volatile("" : "+r"(FP5)); }
    else if constexpr (reg == 6) { FP6 = value; asm volatile("" : "+r"(FP6)); }
    else if constexpr (reg == 7) { FP7 = value; asm volatile("" : "+r"(FP7)); }
}

template <auto Get, auto Set>
struct RegisterProxy {
    using value_type = decltype(Get());
    
    RegisterProxy() = default; 

    operator value_type() const noexcept { return Get(); }
    RegisterProxy& operator=(value_type v) noexcept { Set(v); return *this; }
    RegisterProxy& operator|=(value_type v) noexcept { Set(Get() | v); return *this; }
    RegisterProxy& operator&=(value_type v) noexcept { Set(Get() & v); return *this; }
    RegisterProxy& operator^=(value_type v) noexcept { Set(Get() ^ v); return *this; }

    RegisterProxy(const RegisterProxy&) = delete;
    RegisterProxy& operator=(const RegisterProxy&) = delete;
    void operator&() = delete;
};

inline __attribute__((used)) RegisterProxy<getSR,   setSR>   SR;
inline __attribute__((used)) RegisterProxy<getFPSR, setFPSR> FPSR;
inline __attribute__((used)) RegisterProxy<getFPCR, setFPCR> FPCR;
inline __attribute__((used)) RegisterProxy<getFPIAR, setFPIAR> FPIAR;
inline __attribute__((used)) RegisterProxy<getCACR, setCACR> CACR;
inline __attribute__((used)) RegisterProxy<getUSP, setUSP> USP;
inline __attribute__((used)) RegisterProxy<getMSP, setMSP> MSP;
inline __attribute__((used)) RegisterProxy<getISP, setISP> ISP;

#endif /* __cplusplus */

#endif /* _REGISTERMAPPING_H */
