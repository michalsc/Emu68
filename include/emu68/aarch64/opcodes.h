/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _EMU64_AARCH64_OPCODES_H
#define _EMU64_AARCH64_OPCODES_H

#include <stdint.h>
#include <emu68/RegisterAllocator.h>
#include <emu68/Architectures.h>
#include <type_traits>

namespace emu68 {

enum class CC : uint8_t {
    EQ=0x00, /* Z=1 */
    NE=0x01, /* Z=0 */
    CS=0x02, /* C=1 */
    CC=0x03, /* C=0 */
    MI=0x04, /* N=1 */
    PL=0x05, /* N=0 */
    VS=0x06, /* V=1 */
    VC=0x07, /* V=0 */
    HI=0x08,
    LS=0x09,
    GE=0x0a, /* N == V */
    LT=0x0b, /* N != V */
    GT=0x0c, /* Z == 0 && N == V */
    LE=0x0d, /* Z == 1 || N != V */
    AL=0x0e, /* Always */
    NV=0x0f  /* Always */
};

/* Converts generated ARM instruction to little-endian */
static inline uint32_t I32(uint32_t insn)
{
    union {
        uint32_t u32;
        uint8_t u8[4];
    } val;

    val.u8[3] = (insn >> 24) & 0xff;
    val.u8[2] = (insn >> 16) & 0xff;
    val.u8[1] = (insn >> 8) & 0xff;
    val.u8[0] = (insn)&0xff;

    return val.u32;
}

struct LR {};
struct W32 { static const uint32_t __width_value=0x00000000; };
struct X64 { static const uint32_t __width_value=0x80000000; };
struct SET_FLAGS {};
struct POSTIDX {};
struct PREIDX {};
enum class EXTEND : uint8_t { UXTB = 0, UXTH = 1, UXTW = 2, UXTX = 3, SXTB = 4, SXTH = 5, SXTW = 6, SXTX = 7};
enum class SHIFT : uint8_t { LSL = 0, LSR = 1, ASR = 2, ROR = 3 };
struct LSL_0 {};
struct LSL_1 {};
struct LSL_2 {};
struct LSL_3 {};


/* Branches */
static inline uint32_t B(CC cc, int32_t offset19) { return I32(0x54000000 | (static_cast<int>(cc) & 15) | ((offset19 & 0x7ffff) << 5)); }
template< typename lr=void >
static inline uint32_t B(uint32_t offset) { if (std::is_same<lr, LR>::value) { return I32(0x94000000 | (offset & 0x3ffffff)); } else { return I32(0x14000000 | (offset & 0x3ffffff)); }}
template< typename lr=void >
static inline uint32_t B(const Register<AArch64, INT>& rt) { if (std::is_same<lr, LR>::value) { I32(0xd63f0000 | ((rt.value() & 31) << 5));} else { return I32(0xd61f0000 | ((rt.value() & 31) << 5));}}
template< typename width=W32 >
static inline uint32_t CBNZ(const Register<AArch64, INT>& rt, uint32_t offset19) { return I32(width::__width_value | 0x35000000 | ((offset19 & 0x7ffff) << 5) | (rt.value() & 31)); }
template< typename width=W32 >
static inline uint32_t CBZ(const Register<AArch64, INT>& rt, uint32_t offset19) { return I32(width::__width_value | 0x34000000 | ((offset19 & 0x7ffff) << 5) | (rt.value() & 31)); }
static inline uint32_t RET() { return I32(0xd65f0000 | (30 << 5));}
static inline uint32_t RET(const Register<AArch64, INT>& rt) { return I32(0xd65f0000 | (rt.value() << 5));}
static inline uint32_t TBNZ(const Register<AArch64, INT>& rt, uint8_t bit, uint16_t offset) { return I32(bit & 32 ? 0xb7000000 : 0x37000000 | ((bit & 31) << 19) | ((offset & 0x3fff) << 5) | (rt.value() & 31)); }
static inline uint32_t TBZ(const Register<AArch64, INT>& rt, uint8_t bit, uint16_t offset) { return I32(bit & 32 ? 0xb6000000 : 0x36000000 | ((bit & 31) << 19) | ((offset & 0x3fff) << 5) | (rt.value() & 31)); }

/* System instructions */
template <uint8_t op0, uint8_t op1, uint8_t crn, uint8_t crm, uint8_t op2>
static inline uint32_t MRS(Register<AArch64, INT>& rt) { return I32(0xd5300000 | (rt.value() & 31) | (op0 == 3 ? 0x80000 : 0) | ((op1 & 7) << 16) | ((crn & 15) << 12) | ((crm & 15) << 8) | ((op2 & 7) << 5)); }
template <uint8_t op0, uint8_t op1, uint8_t crn, uint8_t crm, uint8_t op2>
static inline uint32_t MSR(const Register<AArch64, INT>& rt) { return I32(0xd5100000 | (rt.value() & 31) | (op0 == 3 ? 0x80000 : 0) | ((op1 & 7) << 16) | ((crn & 15) << 12) | ((crm & 15) << 8) | ((op2 & 7) << 5)); }
static inline uint32_t BRK(uint16_t imm16) { return I32(0xd4200000 | (imm16 << 5)); }
static inline uint32_t HLT(uint16_t imm16) { return I32(0xd4400000 | (imm16 << 5)); }
static inline uint32_t UDF(uint16_t imm16) { return HLT(imm16); }
static inline uint32_t HINT(uint8_t h) { return I32(0xd503201f | ((h & 0x7f) << 5)); }
static inline uint32_t GET_NZCV(Register<AArch64, INT>& rt) { return MRS<3,3,4,2,0>(rt); }
static inline uint32_t SET_NZCV(const Register<AArch64, INT>& rt) { return MSR<3,3,4,2,0>(rt); }
static inline uint32_t CFINV() { return I32(0xd500401f); }

/* Load PC-relatve address */
static inline uint32_t ADR(Register<AArch64, INT>& rd, uint32_t imm21) { return I32(0x10000000 | (rd.value() & 31) | ((imm21 & 3) << 29) | (((imm21 >> 2) & 0x7ffff) << 5)); }
static inline uint32_t ADRP(Register<AArch64, INT>& rd, uint32_t imm21) { return I32(0x90000000 | (rd.value() & 31) | ((imm21 & 3) << 29) | (((imm21 >> 2) & 0x7ffff) << 5)); }

/* Load/Store instructions */

/* Load/Store PC-relative */
template< typename width=W32 >
static inline uint32_t LDR(Register<AArch64, INT>& rd, int32_t offset19) {
    static_assert(std::is_same<width, W32>::value || std::is_same<width, X64>::value, "Wrong width applied");
    if (std::is_same<width, W32>::value) { return I32(0x18000000 | ((offset19 & 0x7ffff) << 5) | (rd.value() & 31)); }
    else { return I32(0x58000000 | ((offset19 & 0x7ffff) << 5) | (rd.value() & 31)); }
}
static inline uint32_t LDRSW(Register<AArch64, INT>& rd, int32_t offset19) {
    return I32(0x98000000 | ((offset19 & 0x7ffff) << 5) | (rd.value() & 31));
}

/* Load/Store with reg offset */
template< typename width=W32, EXTEND ext=EXTEND::UXTX, typename shift=LSL_0 >
static inline uint32_t LDR(Register<AArch64, INT>& rt, const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm) {
    static_assert((std::is_same<width, W32>::value && (std::is_same<shift, LSL_0>::value || std::is_same<shift, LSL_2>::value)) ||
                  (std::is_same<width, X64>::value && (std::is_same<shift, LSL_0>::value || std::is_same<shift, LSL_3>::value)), "Wrong width/lsl combination");
    if (std::is_same<width, W32>::value) {
        return I32(0xb8600800 | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((rm.value() & 31) << 16) | (std::is_same<shift, LSL_2>::value ? 0x1000 : 0) | (static_cast<int>(ext) << 13));
    } else {
        return I32(0xf8600800 | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((rm.value() & 31) << 16) | (std::is_same<shift, LSL_3>::value ? 0x1000 : 0) | (static_cast<int>(ext) << 13));
    }
}
template<EXTEND ext=EXTEND::UXTX>
static inline uint32_t LDRB(Register<AArch64, INT>& rt, const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm) {
    return I32(0x38600800 | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((rm.value() & 31) << 16) | (static_cast<int>(ext) << 13));
}
template< typename width=W32, EXTEND ext=EXTEND::UXTX >
static inline uint32_t LDRSB(Register<AArch64, INT>& rt, const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm) {
    if (std::is_same<width, W32>::value) {
        return I32(0x38e00800 | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((rm.value() & 31) << 16) | (static_cast<int>(ext) << 13));
    } else {
        return I32(0x38a00800 | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((rm.value() & 31) << 16) | (static_cast<int>(ext) << 13));
    }
}
template<EXTEND ext=EXTEND::UXTX, typename shift=LSL_0 >
static inline uint32_t LDRH(Register<AArch64, INT>& rt, const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm) {
    static_assert(std::is_same<shift, LSL_0>::value || std::is_same<shift, LSL_1>::value, "Wrong shift applied");
    return I32(0x78600800 | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((rm.value() & 31) << 16) | (std::is_same<shift, LSL_1>::value ? 0x1000 : 0) | (static_cast<int>(ext) << 13));
}
template< typename width=W32, EXTEND ext=EXTEND::UXTX, typename shift=LSL_0  >
static inline uint32_t LDRSH(Register<AArch64, INT>& rt, const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm) {
    static_assert(std::is_same<shift, LSL_0>::value || std::is_same<shift, LSL_1>::value, "Wrong shift applied");
    if (std::is_same<width, W32>::value) {
        return I32(0x78e00800 | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((rm.value() & 31) << 16) | (std::is_same<shift, LSL_1>::value ? 0x1000 : 0) | (static_cast<int>(ext) << 13));
    } else {
        return I32(0x78a00800 | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((rm.value() & 31) << 16) | (std::is_same<shift, LSL_1>::value ? 0x1000 : 0) | (static_cast<int>(ext) << 13));
    }
}
template<EXTEND ext=EXTEND::UXTX, typename shift=LSL_0 >
static inline uint32_t LDRSW(Register<AArch64, INT>& rt, const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm) {
    static_assert(std::is_same<shift, LSL_0>::value || std::is_same<shift, LSL_2>::value, "Wrong shift applied");
    return I32(0xb8a00800 | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((rm.value() & 31) << 16) | (std::is_same<shift, LSL_2>::value ? 0x1000 : 0) | (static_cast<int>(ext) << 13));
}

template< typename width=W32, EXTEND ext=EXTEND::UXTX, typename shift=LSL_0 >
static inline uint32_t STR(const Register<AArch64, INT>& rt, const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm) {
    static_assert((std::is_same<width, W32>::value && (std::is_same<shift, LSL_0>::value || std::is_same<shift, LSL_2>::value)) ||
                  (std::is_same<width, X64>::value && (std::is_same<shift, LSL_0>::value || std::is_same<shift, LSL_3>::value)), "Wrong width/lsl combination");
    if (std::is_same<width, W32>::value) {
        return I32(0xb8200800 | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((rm.value() & 31) << 16) | (std::is_same<shift, LSL_2>::value ? 0x1000 : 0) | (static_cast<int>(ext) << 13));
    } else {
        return I32(0xf8200800 | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((rm.value() & 31) << 16) | (std::is_same<shift, LSL_3>::value ? 0x1000 : 0) | (static_cast<int>(ext) << 13));
    }
}
template<EXTEND ext=EXTEND::UXTX>
static inline uint32_t STRB(const Register<AArch64, INT>& rt, const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm) {
    return I32(0x38200800 | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((rm.value() & 31) << 16) | (static_cast<int>(ext) << 13));
}
template<EXTEND ext=EXTEND::UXTX, typename shift=LSL_0 >
static inline uint32_t STRH(const Register<AArch64, INT>& rt, const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm) {
    static_assert(std::is_same<shift, LSL_0>::value || std::is_same<shift, LSL_1>::value, "Wrong shift applied");
    return I32(0x78200800 | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((rm.value() & 31) << 16) | (std::is_same<shift, LSL_1>::value ? 0x1000 : 0) | (static_cast<int>(ext) << 13));
}


/* Load/Store with scaled offset */
template< typename width=W32, typename idx=void >
static inline uint32_t LDR(Register<AArch64, INT>& rt, const Register<AArch64, INT>& rn, int16_t imm) {
    if (std::is_void<idx>::value) { if (std::is_same<width, W32>::value) imm /= 4; else imm /= 8; }
    if (std::is_same<idx, PREIDX>::value) return I32((std::is_same<width, W32>::value ? 0xb8400c00 : 0xf8400c00) | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((imm & 0x1ff) << 12));
    else if (std::is_same<idx, POSTIDX>::value) return I32((std::is_same<width, W32>::value ? 0xb8400400 : 0xf8400400) | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((imm & 0x1ff) << 12));
    else return I32((std::is_same<width, W32>::value ? 0xb9400000 : 0xf9400000) | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((imm & 0xfff) << 10));
}
template< typename idx=void >
static inline uint32_t LDRB(Register<AArch64, INT>& rt, const Register<AArch64, INT>& rn, int16_t imm) {
    if (std::is_same<idx, PREIDX>::value) return I32(0x38400c00 | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((imm & 0x1ff) << 12));
    else if (std::is_same<idx, POSTIDX>::value) return I32(0x38400400 | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((imm & 0x1ff) << 12));
    else return I32(0x39400000 | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((imm & 0xfff) << 10));
}
template< typename width=W32, typename idx=void >
static inline uint32_t LDRSB(Register<AArch64, INT>& rt, const Register<AArch64, INT>& rn, int16_t imm) {
    if (std::is_same<idx, PREIDX>::value) return I32((std::is_same<width, W32>::value ? 0x38c00c00 : 0x38800c00) | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((imm & 0x1ff) << 12));
    else if (std::is_same<idx, POSTIDX>::value) return I32((std::is_same<width, W32>::value ? 0x38c00400 : 0x38800400) | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((imm & 0x1ff) << 12));
    else return I32((std::is_same<width, W32>::value ? 0x39c00000 : 0x39800000) | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((imm & 0xfff) << 10));
}
template< typename idx=void >
static inline uint32_t LDRH(Register<AArch64, INT>& rt, const Register<AArch64, INT>& rn, int16_t imm) {
    if (std::is_void<idx>::value) { imm /= 2; }
    if (std::is_same<idx, PREIDX>::value) return I32(0x78400c00 | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((imm & 0x1ff) << 12));
    else if (std::is_same<idx, POSTIDX>::value) return I32(0x78400400 | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((imm & 0x1ff) << 12));
    else return I32(0x79400000 | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((imm & 0xfff) << 10));
}
template< typename width=W32, typename idx=void >
static inline uint32_t LDRSH(Register<AArch64, INT>& rt, const Register<AArch64, INT>& rn, int16_t imm) {
    if (std::is_void<idx>::value) { imm /= 2; }
    if (std::is_same<idx, PREIDX>::value) return I32((std::is_same<width, W32>::value ? 0x78c00c00 : 0x78800c00) | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((imm & 0x1ff) << 12));
    else if (std::is_same<idx, POSTIDX>::value) return I32((std::is_same<width, W32>::value ? 0x78c00400 : 0x78800400) | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((imm & 0x1ff) << 12));
    else return I32((std::is_same<width, W32>::value ? 0x79c00000 : 0x79800000) | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((imm & 0xfff) << 10));
}
template< typename idx=void >
static inline uint32_t LDRSW(Register<AArch64, INT>& rt, const Register<AArch64, INT>& rn, int16_t imm) {
    if (std::is_void<idx>::value) { imm /= 4; }
    if (std::is_same<idx, PREIDX>::value) return I32(0xb8800c00 | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((imm & 0x1ff) << 12));
    else if (std::is_same<idx, POSTIDX>::value) return I32(0xb8800400 | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((imm & 0x1ff) << 12));
    else return I32(0xb9800000 | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((imm & 0xfff) << 10));
}
template< typename width=W32, typename idx=void >
static inline uint32_t STR(const Register<AArch64, INT>& rt, const Register<AArch64, INT>& rn, int16_t imm) {
    if (std::is_void<idx>::value) { if (std::is_same<width, W32>::value) imm /= 4; else imm /= 8; }
    if (std::is_same<idx, PREIDX>::value) return I32((std::is_same<width, W32>::value ? 0xb8000c00 : 0xf8000c00) | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((imm & 0x1ff) << 12));
    else if (std::is_same<idx, POSTIDX>::value) return I32((std::is_same<width, W32>::value ? 0xb8000400 : 0xf8000400) | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((imm & 0x1ff) << 12));
    else return I32((std::is_same<width, W32>::value ? 0xb9000000 : 0xf9000000) | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((imm & 0xfff) << 10));
}
template< typename idx=void >
static inline uint32_t STRB(const Register<AArch64, INT>& rt, const Register<AArch64, INT>& rn, int16_t imm) {
    if (std::is_same<idx, PREIDX>::value) return I32(0x38000c00 | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((imm & 0x1ff) << 12));
    else if (std::is_same<idx, POSTIDX>::value) return I32(0x38000400 | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((imm & 0x1ff) << 12));
    else return I32(0x39000000 | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((imm & 0xfff) << 10));
}
template< typename idx=void >
static inline uint32_t STRH(const Register<AArch64, INT>& rt, const Register<AArch64, INT>& rn, int16_t imm) {
    if (std::is_void<idx>::value) { imm /= 2; }
    if (std::is_same<idx, PREIDX>::value) return I32(0x78000c00 | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((imm & 0x1ff) << 12));
    else if (std::is_same<idx, POSTIDX>::value) return I32(0x78000400 | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((imm & 0x1ff) << 12));
    else return I32(0x79000000 | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((imm & 0xfff) << 10));
}

/* Load/Store pair */
template< typename width=W32, typename idx=void >
static inline uint32_t LDP(Register<AArch64, INT>& rt1, Register<AArch64, INT>& rt2, const Register<AArch64, INT>& rn, int16_t imm) {
    if (std::is_same<width, W32>::value) imm /= 4; else imm /= 8;
    if (std::is_same<idx, PREIDX>::value) return I32(width::__width_value | 0x29c00000 | (rt1.value() & 31) | ((rt2.value() & 31) << 10) | ((rn.value() & 31) << 5) | ((imm & 0x7f) << 15));
    else if (std::is_same<idx, POSTIDX>::value) return I32(width::__width_value | 0x28c00000 | (rt1.value() & 31) | ((rt2.value() & 31) << 10) | ((rn.value() & 31) << 5) | ((imm & 0x7f) << 15));
    else return I32(width::__width_value | 0x29400000 | (rt1.value() & 31) | ((rt2.value() & 31) << 10) | ((rn.value() & 31) << 5) | ((imm & 0x7f) << 15));
}
template< typename width=W32, typename idx=void >
static inline uint32_t STP(const Register<AArch64, INT>& rt1, const Register<AArch64, INT>& rt2, const Register<AArch64, INT>& rn, int16_t imm) {
    if (std::is_same<width, W32>::value) imm /= 4; else imm /= 8;
    if (std::is_same<idx, PREIDX>::value) return I32(width::__width_value | 0x29800000 | (rt1.value() & 31) | ((rt2.value() & 31) << 10) | ((rn.value() & 31) << 5) | ((imm & 0x7f) << 15));
    else if (std::is_same<idx, POSTIDX>::value) return I32(width::__width_value | 0x28800000 | (rt1.value() & 31) | ((rt2.value() & 31) << 10) | ((rn.value() & 31) << 5) | ((imm & 0x7f) << 15));
    else return I32(width::__width_value | 0x29000000 | (rt1.value() & 31) | ((rt2.value() & 31) << 10) | ((rn.value() & 31) << 5) | ((imm & 0x7f) << 15));
}

/* Load/Store exclusive */
template< typename width=W32 >
static inline uint32_t LDXR(Register<AArch64, INT>& rt, const Register<AArch64, INT>& rn) { return I32((std::is_same<width, W32>::value ? 0x885f7c00 : 0xc85f7c00) | (rt.value() & 31) | ((rn.value() & 31) << 5)); }
static inline uint32_t LDXRB(Register<AArch64, INT>& rt, const Register<AArch64, INT>& rn) { return I32(0x085f7c00 | (rt.value() & 31) | ((rn.value() & 31) << 5)); }
static inline uint32_t LDXRH(Register<AArch64, INT>& rt, const Register<AArch64, INT>& rn) { return I32(0x485f7c00 | (rt.value() & 31) | ((rn.value() & 31) << 5)); }
template< typename width=W32 >
static inline uint32_t STXR(const Register<AArch64, INT>& rt, const Register<AArch64, INT>& rn) { return I32((std::is_same<width, W32>::value ? 0x88007c00 : 0xc8007c00) | (rt.value() & 31) | ((rn.value() & 31) << 5)); }
static inline uint32_t STXRB(const Register<AArch64, INT>& rt, const Register<AArch64, INT>& rn) { return I32(0x08007c00 | (rt.value() & 31) | ((rn.value() & 31) << 5)); }
static inline uint32_t STXRH(const Register<AArch64, INT>& rt, const Register<AArch64, INT>& rn) { return I32(0x48007c00 | (rt.value() & 31) | ((rn.value() & 31) << 5)); }

/* Load/Store with unscaled offset */
template< typename width=W32 >
static inline uint32_t LDUR(Register<AArch64, INT>& rt, const Register<AArch64, INT>& rn, int16_t offset9) { return I32((std::is_same<width, W32>::value ? 0xb8400000 : 0xf8400000) | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((offset9 & 0x1ff) << 12)); }
static inline uint32_t LDURB(Register<AArch64, INT>& rt, const Register<AArch64, INT>& rn, int16_t offset9) { return I32(0x38400000 | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((offset9 & 0x1ff) << 12)); }
template< typename width=W32 >
static inline uint32_t LDURSB(Register<AArch64, INT>& rt, const Register<AArch64, INT>& rn, int16_t offset9) { return I32((std::is_same<width, W32>::value ? 0x38c00000 : 0x38800000) | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((offset9 & 0x1ff) << 12)); }
static inline uint32_t LDURH(Register<AArch64, INT>& rt, const Register<AArch64, INT>& rn, int16_t offset9) { return I32(0x78400000 | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((offset9 & 0x1ff) << 12)); }
template< typename width=W32 >
static inline uint32_t LDURSH(Register<AArch64, INT>& rt, const Register<AArch64, INT>& rn, int16_t offset9) { return I32((std::is_same<width, W32>::value ? 0x78c00000 : 0x78800000) | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((offset9 & 0x1ff) << 12)); }
static inline uint32_t LDURSW(Register<AArch64, INT>& rt, const Register<AArch64, INT>& rn, int16_t offset9) { return I32(0xb8800000 | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((offset9 & 0x1ff) << 12)); }
template< typename width=W32 >
static inline uint32_t STUR(const Register<AArch64, INT>& rt, const Register<AArch64, INT>& rn, int16_t offset9) { return I32((std::is_same<width, W32>::value ? 0xb8000000 : 0xf8000000) | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((offset9 & 0x1ff) << 12)); }
static inline uint32_t STURB(const Register<AArch64, INT>& rt, const Register<AArch64, INT>& rn, int16_t offset9) { return I32(0x38000000 | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((offset9 & 0x1ff) << 12)); }
static inline uint32_t STURH(const Register<AArch64, INT>& rt, const Register<AArch64, INT>& rn, int16_t offset9) { return I32(0x78000000 | (rt.value() & 31) | ((rn.value() & 31) << 5) | ((offset9 & 0x1ff) << 12)); }

/* Data processing: immediate */
template< typename rwidth=W32, typename flags=void >
static inline uint32_t ADD(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, uint16_t imm12) { return I32(rwidth::__width_value | (std::is_same<flags, SET_FLAGS>::value ? 0x31000000 : 0x11000000) | ((imm12 & 0xfff) << 10) | ((rn.value() & 31) << 5) | (rd.value() & 31)); }
template< typename rwidth=W32, typename flags=void >
static inline uint32_t SUB(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, uint16_t imm12) { return I32(rwidth::__width_value | (std::is_same<flags, SET_FLAGS>::value ? 0x71000000 : 0x51000000) | ((imm12 & 0xfff) << 10) | ((rn.value() & 31) << 5) | (rd.value() & 31)); }
template< typename rwidth=W32>
static inline uint32_t CMP(const Register<AArch64, INT>& rn, uint16_t imm12) { Register<AArch64, INT> zr(31); return SUB<rwidth, SET_FLAGS>(zr, rn, imm12); }
template< typename rwidth=W32>
static inline uint32_t CMN(const Register<AArch64, INT>& rn, uint16_t imm12) { Register<AArch64, INT> zr(31); return ADD<rwidth, SET_FLAGS>(zr, rn, imm12); }
template< typename rwidth=W32, typename flags=void >
static inline uint32_t AND(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, uint8_t width, uint8_t ror) { return I32(rwidth::__width_value | (std::is_same<flags, SET_FLAGS>::value ? 0x72000000 : 0x12000000) | (rd.value() & 31) | ((rn.value() & 31) << 5) | (((width - 1) & 0x3f) << 10) | ((ror & 0x3f) << 16)); }
template< typename rwidth=W32, typename flags=void >
static inline uint32_t BIC(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, uint8_t width, uint8_t ror) { return AND<rwidth, flags>(rd, rn, 32-width, ror-width); }
template< typename rwidth=W32 >
static inline uint32_t EOR(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, uint8_t width, uint8_t ror) { return I32(rwidth::__width_value | 0x52000000 | (rd.value() & 31) | ((rn.value() & 31) << 5) | (((width - 1) & 0x3f) << 10) | ((ror & 0x3f) << 16)); }
template< typename rwidth=W32 >
static inline uint32_t ORR(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, uint8_t width, uint8_t ror) { return I32(rwidth::__width_value | 0x32000000 | (rd.value() & 31) | ((rn.value() & 31) << 5) | (((width - 1) & 0x3f) << 10) | ((ror & 0x3f) << 16)); }
template< typename rwidth=W32 >
static inline uint32_t TST(const Register<AArch64, INT>& rn, uint8_t width, uint8_t ror) { Register<AArch64, INT> zr(31); return AND<rwidth, SET_FLAGS>(zr, rn, width, ror); }

/* Data processing: bitfields */
template< typename rwidth=W32 >
static inline uint32_t BFM(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, uint8_t immr, uint8_t imms) { return I32((std::is_same<rwidth, W32>::value ? 0x33000000 : 0xb3400000) | (rd.value() & 31) | ((rn.value() & 31) << 5) | ((immr & 0x3f) << 16) | ((imms & 0x3f) << 10)); }
template< typename rwidth=W32 >
static inline uint32_t SBFM(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, uint8_t immr, uint8_t imms) { return I32((std::is_same<rwidth, W32>::value ? 0x13000000 : 0x93400000) | (rd.value() & 31) | ((rn.value() & 31) << 5) | ((immr & 0x3f) << 16) | ((imms & 0x3f) << 10)); }
template< typename rwidth=W32 >
static inline uint32_t UBFM(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, uint8_t immr, uint8_t imms) { return I32((std::is_same<rwidth, W32>::value ? 0x53000000 : 0xd3400000) | (rd.value() & 31) | ((rn.value() & 31) << 5) | ((immr & 0x3f) << 16) | ((imms & 0x3f) << 10)); }
template< typename rwidth=W32 >
static inline uint32_t BFI(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, uint8_t lsb, uint8_t width) { return BFM<rwidth>(rd, rn, 31 & (32-lsb), width - 1); }
template< typename rwidth=W32 >
static inline uint32_t BFXIL(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, uint8_t lsb, uint8_t width) { return BFM<rwidth>(rd, rn, lsb, lsb + width - 1); }
template< typename rwidth=W32 >
static inline uint32_t SBFX(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, uint8_t lsb, uint8_t width) { return SBFM<rwidth>(rd, rn, lsb, lsb + width - 1); }
template< typename rwidth=W32 >
static inline uint32_t SBFIZ(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, uint8_t lsb, uint8_t width) { return SBFM<rwidth>(rd, rn, 31 & (32-lsb), width - 1); }
template< typename rwidth=W32 >
static inline uint32_t UBFX(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, uint8_t lsb, uint8_t width) { return UBFM<rwidth>(rd, rn, lsb, lsb + width - 1); }
template< typename rwidth=W32 >
static inline uint32_t UBFIZ(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, uint8_t lsb, uint8_t width) { return UBFM<rwidth>(rd, rn, 31 & (32-lsb), width - 1); }

/* Data processing: register extract */
template< typename rwidth=W32 >
static inline uint32_t EXTR(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm, uint8_t lsb) {
    return I32((std::is_same<rwidth, W32>::value ? 0x13800000 : 0x93c00000) | (rd.value() & 31) | ((rn.value() & 31) << 5) | ((rm.value() & 31) << 16) | ((lsb & 63) << 10));
}

/* Data processing: shift immediate */
template< typename rwidth=W32 >
static inline uint32_t ASR(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, uint8_t lsb) { return SBFM<rwidth>(rd, rn, lsb, 31); }
template< typename rwidth=W32 >
static inline uint32_t LSL(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, uint8_t lsb) { return UBFM<rwidth>(rd, rn, 31 & (32 - lsb), 31 - lsb); }
template< typename rwidth=W32 >
static inline uint32_t LSR(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, uint8_t lsb) { return UBFM<rwidth>(rd, rn, lsb, 31); }
template< typename rwidth=W32 >
static inline uint32_t ROR(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, uint8_t lsb) { return EXTR<rwidth>(rd, rn, rn, lsb); }

/* Data processing: extending */
template< typename rwidth=W32 >
static inline uint32_t SXTB(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn) { return SBFM<rwidth>(rd, rn, 0, 7); }
template< typename rwidth=W32 >
static inline uint32_t SXTH(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn) { return SBFM<rwidth>(rd, rn, 0, 15); }
static inline uint32_t SXTW(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn) { return SBFM<X64>(rd, rn, 0, 31); }
template< typename rwidth=W32 >
static inline uint32_t UXTB(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn) { return UBFM<rwidth>(rd, rn, 0, 7); }
template< typename rwidth=W32 >
static inline uint32_t UXTH(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn) { return UBFM<rwidth>(rd, rn, 0, 15); }

/* Data processing: move */
template< typename rwidth=W32, uint8_t shift16=0>
static inline uint32_t MOV(Register<AArch64, INT>& rd, uint16_t val) {
    static_assert((std::is_same<rwidth, W32>::value && shift16 < 2) || (std::is_same<rwidth, X64>::value && shift16 < 4), "Wrong width/shift combination");
    return I32((std::is_same<rwidth, W32>::value ? 0x52800000 : 0xd2800000) | ((shift16 & 3) << 21) | (val << 5) | (rd.value() & 31));
}
template< typename rwidth=W32, uint8_t shift16=0>
static inline uint32_t MOVK(Register<AArch64, INT>& rd, uint16_t val) {
    static_assert((std::is_same<rwidth, W32>::value && shift16 < 2) || (std::is_same<rwidth, X64>::value && shift16 < 4), "Wrong width/shift combination");
    return I32((std::is_same<rwidth, W32>::value ? 0x72800000 : 0xf2800000) | ((shift16 & 3) << 21) | (val << 5) | (rd.value() & 31));
}
template< typename rwidth=W32, uint8_t shift16=0>
static inline uint32_t MOVN(Register<AArch64, INT>& rd, uint16_t val) {
    static_assert((std::is_same<rwidth, W32>::value && shift16 < 2) || (std::is_same<rwidth, X64>::value && shift16 < 4), "Wrong width/shift combination");
    return I32((std::is_same<rwidth, W32>::value ? 0x12800000 : 0x92800000) | ((shift16 & 3) << 21) | (val << 5) | (rd.value() & 31));
}
template< typename rwidth=W32 >
static inline uint32_t MOVS(Register<AArch64, INT>& rd, int16_t val) {
    if (val < 0) return MOVN<rwidth>(rd, -val - 1); else return MOV<rwidth>(rd, val);
}

/* Data processing: register */
template< typename width=W32, SHIFT shift=SHIFT::LSL, uint8_t amount=0>
static inline uint32_t ADD(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm) { return I32(width::__width_value | 0x0b000000 | (static_cast<int>(shift) << 22) | (rd.value() & 31) | ((rn.value() & 31) << 5) | ((rm.value() & 31) << 16) | ((amount & 63) << 10)); }
template< typename width=W32, SHIFT shift=SHIFT::LSL, uint8_t amount=0>
static inline uint32_t ADDS(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm) { return I32(width::__width_value | 0x2b000000 | (static_cast<int>(shift) << 22) | (rd.value() & 31) | ((rn.value() & 31) << 5) | ((rm.value() & 31) << 16) | ((amount & 63) << 10)); }
template< typename width=W32, SHIFT shift=SHIFT::LSL, uint8_t amount=0>
static inline uint32_t SUB(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm) { return I32(width::__width_value | 0x4b000000 | (static_cast<int>(shift) << 22) | (rd.value() & 31) | ((rn.value() & 31) << 5) | ((rm.value() & 31) << 16) | ((amount & 63) << 10)); }
template< typename width=W32, SHIFT shift=SHIFT::LSL, uint8_t amount=0>
static inline uint32_t SUBS(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm) { return I32(width::__width_value | 0x6b000000 | (static_cast<int>(shift) << 22) | (rd.value() & 31) | ((rn.value() & 31) << 5) | ((rm.value() & 31) << 16) | ((amount & 63) << 10)); }
template< typename width=W32, SHIFT shift=SHIFT::LSL, uint8_t amount=0>
static inline uint32_t CMN(const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm) { Register<AArch64, INT> zr(31); return ADDS<width, shift, amount>(zr, rn, rm); }
template< typename width=W32, SHIFT shift=SHIFT::LSL, uint8_t amount=0>
static inline uint32_t CMP(const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm) { Register<AArch64, INT> zr(31); return SUBS<width, shift, amount>(zr, rn, rm); }
template< typename width=W32, SHIFT shift=SHIFT::LSL, uint8_t amount=0>
static inline uint32_t NEG(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rm) { Register<AArch64, INT> zr(31); return SUB<width, shift, amount>(rd, zr, rm); }
template< typename width=W32, SHIFT shift=SHIFT::LSL, uint8_t amount=0>
static inline uint32_t NEGS(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rm) { Register<AArch64, INT> zr(31); return SUBS<width, shift, amount>(rd, zr, rm); }

#if 0

/* Data processing: register with extend */
static inline uint32_t add_reg_ext(uint8_t rd, uint8_t rn, uint8_t rm, reg_extend_t extend, uint8_t lsl) { return I32(0x0b200000 | ((lsl & 7) << 10) | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | ((extend & 7) << 13)); }
static inline uint32_t add64_reg_ext(uint8_t rd, uint8_t rn, uint8_t rm, reg_extend_t extend, uint8_t lsl) { return I32(0x8b200000 | ((lsl & 7) << 10) | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | ((extend & 7) << 13)); }
static inline uint32_t adds_reg_ext(uint8_t rd, uint8_t rn, uint8_t rm, reg_extend_t extend, uint8_t lsl) { return I32(0x2b200000 | ((lsl & 7) << 10) | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | ((extend & 7) << 13)); }
static inline uint32_t adds64_reg_ext(uint8_t rd, uint8_t rn, uint8_t rm, reg_extend_t extend, uint8_t lsl) { return I32(0xab200000 | ((lsl & 7) << 10) | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | ((extend & 7) << 13)); }
static inline uint32_t sub_reg_ext(uint8_t rd, uint8_t rn, uint8_t rm, reg_extend_t extend, uint8_t lsl) { return I32(0x4b200000 | ((lsl & 7) << 10) | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | ((extend & 7) << 13)); }
static inline uint32_t sub64_reg_ext(uint8_t rd, uint8_t rn, uint8_t rm, reg_extend_t extend, uint8_t lsl) { return I32(0xcb200000 | ((lsl & 7) << 10) | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | ((extend & 7) << 13)); }
static inline uint32_t subs_reg_ext(uint8_t rd, uint8_t rn, uint8_t rm, reg_extend_t extend, uint8_t lsl) { return I32(0x6b200000 | ((lsl & 7) << 10) | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | ((extend & 7) << 13)); }
static inline uint32_t subs64_reg_ext(uint8_t rd, uint8_t rn, uint8_t rm, reg_extend_t extend, uint8_t lsl) { return I32(0xeb200000 | ((lsl & 7) << 10) | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | ((extend & 7) << 13)); }
static inline uint32_t cmn_reg_ext(uint8_t rn, uint8_t rm, reg_extend_t extend, uint8_t lsl) { return adds_reg_ext(31, rn, rm, extend, lsl); }
static inline uint32_t cmn64_reg_ext(uint8_t rn, uint8_t rm, reg_extend_t extend, uint8_t lsl) { return adds64_reg_ext(31, rn, rm, extend, lsl); }
static inline uint32_t cmp_reg_ext(uint8_t rn, uint8_t rm, reg_extend_t extend, uint8_t lsl) { return subs_reg_ext(31, rn, rm, extend, lsl); }
static inline uint32_t cmp64_reg_ext(uint8_t rn, uint8_t rm, reg_extend_t extend, uint8_t lsl) { return subs64_reg_ext(31, rn, rm, extend, lsl); }
static inline uint32_t neg_reg_ext(uint8_t rd, uint8_t rm, reg_extend_t extend, uint8_t lsl) { return sub_reg_ext(rd, 31, rm, extend, lsl); }
static inline uint32_t neg64_reg_ext(uint8_t rd, uint8_t rm, reg_extend_t extend, uint8_t lsl) { return sub64_reg_ext(rd, 31, rm, extend, lsl); }
static inline uint32_t negs_reg_ext(uint8_t rd, uint8_t rm, reg_extend_t extend, uint8_t lsl) { return subs_reg_ext(rd, 31, rm, extend, lsl); }
static inline uint32_t negs64_reg_ext(uint8_t rd, uint8_t rm, reg_extend_t extend, uint8_t lsl) { return subs64_reg_ext(rd, 31, rm, extend, lsl); }

#endif

/* Data prcessing: arithmetic with carry */
template< typename width=W32 >
static inline uint32_t ADC(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm) { return I32(width::__width_value | 0x1a000000 | (rd.value() & 31) | ((rn.value() & 31) << 5) | ((rm.value() & 31) << 16)); }
template< typename width=W32 >
static inline uint32_t ADCS(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm) { return I32(width::__width_value | 0x3a000000 | (rd.value() & 31) | ((rn.value() & 31) << 5) | ((rm.value() & 31) << 16)); }
template< typename width=W32 >
static inline uint32_t SBC(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm) { return I32(width::__width_value | 0x5a000000 | (rd.value() & 31) | ((rn.value() & 31) << 5) | ((rm.value() & 31) << 16)); }
template< typename width=W32 >
static inline uint32_t SBCS(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm) { return I32(width::__width_value | 0x7a000000 | (rd.value() & 31) | ((rn.value() & 31) << 5) | ((rm.value() & 31) << 16)); }
template< typename width=W32 >
static inline uint32_t NGC(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rm) { Register<AArch64, INT> zr(31); return SBC<width>(rd, zr, rm); }
template< typename width=W32 >
static inline uint32_t NGCS(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rm) { Register<AArch64, INT> zr(31); return SBCS<width>(rd, zr, rm); }


/* Data processing: conditional select */
template< typename width=W32 >
static inline uint32_t CSEL(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm, CC cond) { return I32(width::__width_value | 0x1a800000 | (rd.value() & 31) | ((rn.value() & 31) << 5) | ((rm.value() & 31) << 16) | (static_cast<int>(cond)  << 12)); }
template< typename width=W32 >
static inline uint32_t CSINC(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm, CC cond) { return I32(width::__width_value | 0x1a800400 | (rd.value() & 31) | ((rn.value() & 31) << 5) | ((rm.value() & 31) << 16) | (static_cast<int>(cond)  << 12)); }
template< typename width=W32 >
static inline uint32_t CSINV(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm, CC cond) { return I32(width::__width_value | 0x5a800000 | (rd.value() & 31) | ((rn.value() & 31) << 5) | ((rm.value() & 31) << 16) | (static_cast<int>(cond)  << 12)); }
template< typename width=W32 >
static inline uint32_t CSETM(Register<AArch64, INT>& rd, CC cond) { Register<AArch64, INT> zr(31); return CSINV<width>(rd, zr, zr, static_cast<CC>(static_cast<int>(cond) ^ 1)); }
template< typename width=W32 >
static inline uint32_t CSET(Register<AArch64, INT>& rd, CC cond) { Register<AArch64, INT> zr(31); return CSINC<width>(rd, zr, zr, static_cast<CC>(static_cast<int>(cond) ^ 1)); }

/* Data processing: logic */
template< typename width=W32, SHIFT shift=SHIFT::LSL, uint8_t amount=0>
static inline uint32_t AND(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm) { return I32(width::__width_value | 0x0a000000 | (static_cast<int>(shift) << 22) | (rd.value() & 31) | ((rn.value() & 31) << 5) | ((rm.value() & 31) << 16) | ((amount & 63) << 10)); }
template< typename width=W32, SHIFT shift=SHIFT::LSL, uint8_t amount=0>
static inline uint32_t ANDS(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm) { return I32(width::__width_value | 0x6a000000 | (static_cast<int>(shift) << 22) | (rd.value() & 31) | ((rn.value() & 31) << 5) | ((rm.value() & 31) << 16) | ((amount & 63) << 10)); }
template< typename width=W32, SHIFT shift=SHIFT::LSL, uint8_t amount=0>
static inline uint32_t BIC(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm) { return I32(width::__width_value | 0x0a200000 | (static_cast<int>(shift) << 22) | (rd.value() & 31) | ((rn.value() & 31) << 5) | ((rm.value() & 31) << 16) | ((amount & 63) << 10)); }
template< typename width=W32, SHIFT shift=SHIFT::LSL, uint8_t amount=0>
static inline uint32_t BICS(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm) { return I32(width::__width_value | 0x6a200000 | (static_cast<int>(shift) << 22) | (rd.value() & 31) | ((rn.value() & 31) << 5) | ((rm.value() & 31) << 16) | ((amount & 63) << 10)); }
template< typename width=W32, SHIFT shift=SHIFT::LSL, uint8_t amount=0>
static inline uint32_t EON(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm) { return I32(width::__width_value | 0x4a200000 | (static_cast<int>(shift) << 22) | (rd.value() & 31) | ((rn.value() & 31) << 5) | ((rm.value() & 31) << 16) | ((amount & 63) << 10)); }
template< typename width=W32, SHIFT shift=SHIFT::LSL, uint8_t amount=0>
static inline uint32_t EOR(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm) { return I32(width::__width_value | 0x4a000000 | (static_cast<int>(shift) << 22) | (rd.value() & 31) | ((rn.value() & 31) << 5) | ((rm.value() & 31) << 16) | ((amount & 63) << 10)); }
template< typename width=W32, SHIFT shift=SHIFT::LSL, uint8_t amount=0>
static inline uint32_t ORR(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm) { return I32(width::__width_value | 0x2a000000 | (static_cast<int>(shift) << 22) | (rd.value() & 31) | ((rn.value() & 31) << 5) | ((rm.value() & 31) << 16) | ((amount & 63) << 10)); }
template< typename width=W32, SHIFT shift=SHIFT::LSL, uint8_t amount=0>
static inline uint32_t ORN(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm) { return I32(width::__width_value | 0x2a200000 | (static_cast<int>(shift) << 22) | (rd.value() & 31) | ((rn.value() & 31) << 5) | ((rm.value() & 31) << 16) | ((amount & 63) << 10)); }
template< typename width=W32, SHIFT shift=SHIFT::LSL, uint8_t amount=0>
static inline uint32_t MVN(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rm) { Register<AArch64, INT> zr(31); return ORN<width, shift, amount>(rd, zr, rm); }
template< typename width=W32, SHIFT shift=SHIFT::LSL, uint8_t amount=0>
static inline uint32_t TST(const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm) { Register<AArch64, INT> zr(31); return ANDS<width, shift, amount>(zr, rn, rm); }

/* Data processing: move register */
template< typename width=W32 >
static inline uint32_t MOV(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rm) { Register<AArch64, INT> zr(31); return ORR<width>(rd, ZR, rm); }

/* Data processing: shift reg */
template< typename width=W32 >
static inline uint32_t ASR(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm) { return I32(width::__width_value | 0x1ac02800 | (rd.value() & 31) | ((rn.value() & 31) << 5) | ((rm.value() & 31) << 16)); }
template< typename width=W32 >
static inline uint32_t LSL(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm) { return I32(width::__width_value | 0x1ac02000 | (rd.value() & 31) | ((rn.value() & 31) << 5) | ((rm.value() & 31) << 16)); }
template< typename width=W32 >
static inline uint32_t LSR(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm) { return I32(width::__width_value | 0x1ac02400 | (rd.value() & 31) | ((rn.value() & 31) << 5) | ((rm.value() & 31) << 16)); }
template< typename width=W32 >
static inline uint32_t ROR(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm) { return I32(width::__width_value | 0x1ac02c00 | (rd.value() & 31) | ((rn.value() & 31) << 5) | ((rm.value() & 31) << 16)); }

/* Data processing: multiply */
static inline uint32_t SMADDL(Register<AArch64, INT>& rd, const Register<AArch64, INT>& ra, const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm) { return I32(0x9b200000 | (rd.value() & 31) | ((rn.value() & 31) << 5) | ((ra.value() & 31) << 10) | ((rm.value() & 31) << 16)); }
static inline uint32_t SMSUBL(Register<AArch64, INT>& rd, const Register<AArch64, INT>& ra, const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm) { return I32(0x9b208000 | (rd.value() & 31) | ((rn.value() & 31) << 5) | ((ra.value() & 31) << 10) | ((rm.value() & 31) << 16)); }
static inline uint32_t SMNEGL(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm) { Register<AArch64, INT> zr(31); return SMSUBL(rd, zr, rn, rm); }
static inline uint32_t SMULL(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm) { Register<AArch64, INT> zr(31); return SMADDL(rd, zr, rn, rm); }
static inline uint32_t UMADDL(Register<AArch64, INT>& rd, const Register<AArch64, INT>& ra, const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm) { return I32(0x9ba00000 | (rd.value() & 31) | ((rn.value() & 31) << 5) | ((ra.value() & 31) << 10) | ((rm.value() & 31) << 16)); }
static inline uint32_t UMSUBL(Register<AArch64, INT>& rd, const Register<AArch64, INT>& ra, const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm) { return I32(0x9ba08000 | (rd.value() & 31) | ((rn.value() & 31) << 5) | ((ra.value() & 31) << 10) | ((rm.value() & 31) << 16)); }
static inline uint32_t UMNEGL(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm) { Register<AArch64, INT> zr(31); return UMSUBL(rd, zr, rn, rm); }
static inline uint32_t UMULL(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm) { Register<AArch64, INT> zr(31); return UMADDL(rd, zr, rn, rm); }

template< typename width=W32 >
static inline uint32_t MADD(Register<AArch64, INT>& rd, const Register<AArch64, INT>& ra, const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm) { return I32(width::__width_value | 0x1b000000 | (rd.value() & 31) | ((rn.value() & 31) << 5) | ((ra.value() & 31) << 10) | ((rm.value() & 31) << 16)); }
template< typename width=W32 >
static inline uint32_t MSUB(Register<AArch64, INT>& rd, const Register<AArch64, INT>& ra, const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm) { return I32(width::__width_value | 0x1b008000 | (rd.value() & 31) | ((rn.value() & 31) << 5) | ((ra.value() & 31) << 10) | ((rm.value() & 31) << 16)); }
template< typename width=W32 >
static inline uint32_t MNEG(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm) { Register<AArch64, INT> zr(31); return MSUB<width>(rd, zr, rn, rm); }
template< typename width=W32 >
static inline uint32_t MUL(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm) { Register<AArch64, INT> zr(31); return MADD<width>(rd, zr, rn, rm); }

/* Data processing: divide */
template< typename width=W32 >
static inline uint32_t SDIV(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm) { return I32(width::__width_value | 0x1ac00c00 | (rd.value() & 31) | ((rn.value() & 31) << 5) | ((rm.value() & 31) << 16)); }
template< typename width=W32 >
static inline uint32_t UDIV(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn, const Register<AArch64, INT>& rm) { return I32(width::__width_value | 0x1ac00800 | (rd.value() & 31) | ((rn.value() & 31) << 5) | ((rm.value() & 31) << 16)); }

/* Data processing: bit operations */
template< typename width=W32 >
static inline uint32_t CLS(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn) { return I32(width::__width_value | 0x5ac01400 | (rd.value() & 31) | ((rn.value() & 31) << 5)); }
template< typename width=W32 >
static inline uint32_t CLZ(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn) { return I32(width::__width_value | 0x5ac01000 | (rd.value() & 31) | ((rn.value() & 31) << 5)); }
template< typename width=W32 >
static inline uint32_t RBIT(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn) { return I32(width::__width_value | 0x5ac00000 | (rd.value() & 31) | ((rn.value() & 31) << 5)); }
template< typename width=W32 >
static inline uint32_t REV(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn) { return I32((width::__width_value ? 0xdac00c0 : 0x5ac00800) | (rd.value() & 31) | ((rn.value() & 31) << 5)); }
template< typename width=W32 >
static inline uint32_t REV16(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn) { return I32(width::__width_value | 0x5ac00400 | (rd.value() & 31) | ((rn.value() & 31) << 5)); }
static inline uint32_t REV32(Register<AArch64, INT>& rd, const Register<AArch64, INT>& rn) { return I32(0xdac00800 | (rd.value() & 31) | ((rn.value() & 31) << 5)); }

/* FPU/SIMD */
static inline uint32_t ABS(Register<AArch64, DOUBLE>& rd, const Register<AArch64, DOUBLE>& rn) { return I32(0x1e60c000 | (rd.value() & 31) | ((rn.value() & 31) << 5)); }
static inline uint32_t ABS(Register<AArch64, SINGLE>& rd, const Register<AArch64, SINGLE>& rn) { return I32(0x1e20c000 | (rd.value() & 31) | ((rn.value() & 31) << 5)); }
static inline uint32_t ADD(Register<AArch64, DOUBLE>& rd, const Register<AArch64, DOUBLE>& first, const Register<AArch64, DOUBLE>& second) { return I32(0x1e602800 | (rd.value() & 31) | ((first.value() & 31) << 5) | ((second.value() & 31) << 16)); }
static inline uint32_t ADD(Register<AArch64, SINGLE>& rd, const Register<AArch64, SINGLE>& first, const Register<AArch64, SINGLE>& second) { return I32(0x1e202800 | (rd.value() & 31) | ((first.value() & 31) << 5) | ((second.value() & 31) << 16)); }
static inline uint32_t CMP(const Register<AArch64, DOUBLE>& rd, const Register<AArch64, DOUBLE>& rn) { return I32(0x1e602000 | ((rd.value() & 31) << 5) | ((rn.value() & 31) << 16)); }
static inline uint32_t CMP(const Register<AArch64, SINGLE>& rd, const Register<AArch64, SINGLE>& rn) { return I32(0x1e202000 | ((rd.value() & 31) << 5) | ((rn.value() & 31) << 16)); }
static inline uint32_t CMPZ(const Register<AArch64, DOUBLE>& rn) { return INSN_TO_LE(0x1e602008 | ((rn.value() & 31) << 5)); }
static inline uint32_t CMPZ(const Register<AArch64, SINGLE>& rn) { return INSN_TO_LE(0x1e202008 | ((rn.value() & 31) << 5)); }
template< typename width=W32 >
static inline uint32_t CVT(Register<AArch64, DOUBLE>& rd, const Register<AArch64, INT>& rn) { return I32(width::__width_value | 0x1e620000 | (rd.value() & 31) | ((rn.value() & 31) << 5)); }
template< typename width=W32 >
static inline uint32_t CVT(Register<AArch64, SINGLE>& rd, const Register<AArch64, INT>& rn) { return I32(width::__width_value | 0x1e220000 | (rd.value() & 31) | ((rn.value() & 31) << 5)); }
template< typename width=W32 >
static inline uint32_t CVT(Register<AArch64, INT>& rd, const Register<AArch64, DOUBLE>& rn) { return I32(width::__width_value | 0x1e780000 | (rd.value() & 31) | ((rn.value() & 31) << 5)); }
template< typename width=W32 >
static inline uint32_t CVT(Register<AArch64, INT>& rd, const Register<AArch64, SINGLE>& rn) { return I32(width::__width_value | 0x1e380000 | (rd.value() & 31) | ((rn.value() & 31) << 5)); }
static inline uint32_t DIV(Register<AArch64, DOUBLE>& rd, const Register<AArch64, DOUBLE>& dividend, const Register<AArch64, DOUBLE>& divisor) { return I32(0x1e601800 | (rd.value() & 31) | ((dividend.value() & 31) << 5) | ((divisor.value() & 31) << 16)); }
static inline uint32_t FRINT64X(Register<AArch64, DOUBLE>& rd, const Register<AArch64, DOUBLE>& rn) { return I32(0x1e67c000 | (rd.value() & 31) | ((rn.value() & 31) << 5)); }
static inline uint32_t FRINT64Z(Register<AArch64, DOUBLE>& rd, const Register<AArch64, DOUBLE>& rn) { return I32(0x1e65c000 | (rd.value() & 31) | ((rn.value() & 31) << 5)); }

template< typename scale=LSL_0 >
static inline uint32_t LDR(Register<AArch64, DOUBLE>& rd, const Register<AArch64, INT>& base, int16_t offset) {
    static_assert(std::is_same<scale, LSL_0>::value || std::is_same<scale, LSL_3>::value, "Scale can be only LSL_0 or LSL_3");
    if (std::is_same<scale, LSL_3>::value) { return I32(0xfd400000 | ((base.value() & 31) << 5) | (rd.value() & 31) | ((offset & 0xfff) << 10));
    } else { return I32(0xfc400000 | ((base.value() & 31) << 5) | (rd.value() & 31) | ((offset & 0x1ff) << 12)); } }
template< typename scale=LSL_0 >
static inline uint32_t LDR(Register<AArch64, SINGLE>& rd, const Register<AArch64, INT>& base, int16_t offset) {
    static_assert(std::is_same<scale, LSL_0>::value || std::is_same<scale, LSL_2>::value, "Scale can be only LSL_0 or LSL_2");
    if (std::is_same<scale, LSL_2>::value) { return I32(0xbd400000 | ((base.value() & 31) << 5) | (rd.value() & 31) | ((offset & 0xfff) << 10));
    } else { return I32(0xbc400000 | ((base.value() & 31) << 5) | (rd.value() & 31) | ((offset & 0x1ff) << 12)); } }
static inline uint32_t LDR(Register<AArch64, DOUBLE>& rd, int32_t offset) { return I32(0x5c000000 | (rd.value() & 31) | ((offset & 0x7ffff) << 5)); }
static inline uint32_t LDR(Register<AArch64, SINGLE>& rd, int32_t offset) { return I32(0x1c000000 | (rd.value() & 31) | ((offset & 0x7ffff) << 5)); }

static inline uint32_t MOV(Register<AArch64, DOUBLE>& rd, const Register<AArch64, DOUBLE>& rn) { return I32(0x1e604000 | (rd.value() & 31) | ((rn.value() & 31) << 5)); }
static inline uint32_t MOV(Register<AArch64, SINGLE>& rd,const  Register<AArch64, SINGLE>& rn) { return I32(0x1e204000 | (rd.value() & 31) | ((rn.value() & 31) << 5)); }
static inline uint32_t MOV(Register<AArch64, DOUBLE>& rd, const Register<AArch64, SINGLE>& rn) { return I32(0x1e22c000 | (rd.value() & 31) | ((rn.value() & 31) << 5)); }
static inline uint32_t MOV(Register<AArch64, SINGLE>& rd, const Register<AArch64, DOUBLE>& rn) { return I32(0x1e624000 | (rd.value() & 31) | ((rn.value() & 31) << 5)); }
enum class TS : uint8_t { B = 1, H = 2, S = 4, D = 8 };
template < TS ts=TS::D, int index=0 >
static inline uint32_t MOV(Register<AArch64, INT>& rd, const Register<AArch64, DOUBLE>& rn) { return I32((ts == TS::D ? 0x4e003c00 : 0x0e003c00) | (ts == TS::B ? ((index & 0xf) << 17) : ts == TS::H ? ((index & 7) << 18) : ts == TS::S ? ((index & 3) << 19) : ts == TS::D ? ((index & 1) << 20) : 0) | (static_cast<uint8_t>(ts) << 16) | (rd.value() & 31) | ((rn.value() & 31) << 5)); }
template < TS ts=TS::S, int index=0 >
static inline uint32_t MOV(Register<AArch64, INT>& rd, const Register<AArch64, SINGLE>& rn) { return I32((ts == TS::D ? 0x4e003c00 : 0x0e003c00) | (ts == TS::B ? ((index & 0xf) << 17) : ts == TS::H ? ((index & 7) << 18) : ts == TS::S ? ((index & 3) << 19) : ts == TS::D ? ((index & 1) << 20) : 0) | (static_cast<uint8_t>(ts) << 16) | (rd.value() & 31) | ((rn.value() & 31) << 5)); }
template < TS ts=TS::D, int index=0 >
static inline uint32_t MOV(Register<AArch64, DOUBLE>& rd, const Register<AArch64, INT>& rn) { return I32(0x4e001c00 | (ts == TS::B ? ((index & 0xf) << 17) : ts == TS::H ? ((index & 7) << 18) : ts == TS::S ? ((index & 3) << 19) : ts == TS::D ? ((index & 1) << 20) : 0) | (static_cast<uint8_t>(ts) << 16) | (rd.value() & 31) | ((rn.value() & 31) << 5)); }
template < TS ts=TS::S, int index=0 >
static inline uint32_t MOV(Register<AArch64, SINGLE>& rd, const Register<AArch64, INT>& rn) { return I32(0x4e001c00 | (ts == TS::B ? ((index & 0xf) << 17) : ts == TS::H ? ((index & 7) << 18) : ts == TS::S ? ((index & 3) << 19) : ts == TS::D ? ((index & 1) << 20) : 0) | (static_cast<uint8_t>(ts) << 16) | (rd.value() & 31) | ((rn.value() & 31) << 5)); }
static inline uint32_t MOV(Register<AArch64, DOUBLE>& rd, uint8_t imm) { return I32(0x1e601000 | (imm << 13) | (rd.value() & 31)); }
static inline uint32_t MOV(Register<AArch64, SINGLE>& rd, uint8_t imm) { return I32(0x1e201000 | (imm << 13) | (rd.value() & 31)); }
static inline uint32_t MOVI(Register<AArch64, DOUBLE>& rd, uint8_t imm) { return I32(0x2f00e400 | (rd.value() & 31) | ((imm & 31) << 5) | (((imm > 5) & 7) << 16)); }
static inline uint32_t MOV_0(Register<AArch64, DOUBLE>& rd) { return MOVI(rd, 0); }
static inline uint32_t MOV_1(Register<AArch64, DOUBLE>& rd) { return MOV(rd, 112); }
static inline uint32_t MUL(Register<AArch64, DOUBLE>& rd, const Register<AArch64, DOUBLE>& first, const Register<AArch64, DOUBLE>& second) { return I32(0x1e600800 | (rd.value() & 31) | ((first.value() & 31) << 5) | ((second.value() & 31) << 16)); }
static inline uint32_t NEG(Register<AArch64, DOUBLE>& rd, const Register<AArch64, DOUBLE>& rn) { return I32(0x1e614000 | (rd.value() & 31) | ((rn.value() & 31) << 5)); }
static inline uint32_t SQRT(Register<AArch64, DOUBLE>& rd, const Register<AArch64, DOUBLE>& rn) { return I32(0x1e61c000 | (rd.value() & 31) | ((rn.value() & 31) << 5)); }
static inline uint32_t SUB(Register<AArch64, DOUBLE>& rd, const Register<AArch64, DOUBLE>& first, const Register<AArch64, DOUBLE>& second) { return I32(0x1e603800 | (rd.value() & 31) | ((first.value() & 31) << 5) | ((second.value() & 31) << 16)); }
template< typename scale=LSL_0 >
static inline uint32_t STR(const Register<AArch64, DOUBLE>& rd, const Register<AArch64, INT>& base, int16_t offset) {
    static_assert(std::is_same<scale, LSL_0>::value || std::is_same<scale, LSL_3>::value, "Scale can be only LSL_0 or LSL_3");
    if (std::is_same<scale, LSL_3>::value) { return I32(0xfd000000 | ((base.value() & 31) << 5) | (rd.value() & 31) | ((offset & 0xfff) << 10));
    } else { return I32(0xfc000000 | ((base.value() & 31) << 5) | (rd.value() & 31) | ((offset & 0x1ff) << 12)); } }
template< typename scale=void >
static inline uint32_t STR(const Register<AArch64, SINGLE>& rd, const Register<AArch64, INT>& base, int16_t offset) {
    static_assert(std::is_same<scale, LSL_0>::value || std::is_same<scale, LSL_2>::value, "Scale can be only LSL_0 or LSL_2");
    if (std::is_same<scale, LSL_2>::value) { return I32(0xbd000000 | ((base.value() & 31) << 5) | (rd.value() & 31) | ((offset & 0xfff) << 10));
    } else { return I32(0xbc000000 | ((base.value() & 31) << 5) | (rd.value() & 31) | ((offset & 0x1ff) << 12)); } }


}

#endif /* _EMU64_AARCH64_OPCODES_H */
