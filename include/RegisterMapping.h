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

#ifdef __cplusplus

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
//inline RegisterProxy<getFPSR, setFPSR> FPSR;
inline __attribute__((used)) RegisterProxy<getCACR, setCACR> CACR;
inline __attribute__((used)) RegisterProxy<getUSP, setUSP> USP;
inline __attribute__((used)) RegisterProxy<getMSP, setMSP> MSP;
inline __attribute__((used)) RegisterProxy<getISP, setISP> ISP;

#endif /* __cplusplus */

#endif /* _REGISTERMAPPING_H */
