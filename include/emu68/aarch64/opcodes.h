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
struct SW { };
struct OFFSET_SCALED {};

/* Branches */
template< CC cc >
static inline uint32_t B(int32_t offset19) { return I32(0x54000000 | (static_cast<int>(cc) & 15) | ((offset19 & 0x7ffff) << 5)); }
template< typename lr=void >
static inline uint32_t B(uint32_t offset) { if (std::is_same<lr, LR>::value) { return I32(0x94000000 | (offset & 0x3ffffff)); } else { return I32(0x14000000 | (offset & 0x3ffffff)); }}
template< typename lr=void >
static inline uint32_t B(Register<AArch64, INT> rt) { if (std::is_same<lr, LR>::value) { I32(0xd63f0000 | ((rt.value() & 31) << 5));} else { return I32(0xd61f0000 | ((rt.value() & 31) << 5));}}
template< typename width=W32 >
static inline uint32_t CBNZ(Register<AArch64, INT> rt, uint32_t offset19) { return I32(width::__width_value | 0x35000000 | ((offset19 & 0x7ffff) << 5) | (rt.value() & 31)); }
template< typename width=W32 >
static inline uint32_t CBZ(Register<AArch64, INT> rt, uint32_t offset19) { return I32(width::__width_value | 0x34000000 | ((offset19 & 0x7ffff) << 5) | (rt.value() & 31)); } 
static inline uint32_t RET() { return I32(0xd65f0000 | (30 << 5));}
static inline uint32_t RET(Register<AArch64, INT> rt) { return I32(0xd65f0000 | (rt.value() << 5));}
static inline uint32_t TBNZ(Register<AArch64, INT> rt, uint8_t bit, uint16_t offset) { return I32(bit & 32 ? 0xb7000000 : 0x37000000 | ((bit & 31) << 19) | ((offset & 0x3fff) << 5) | (rt.value() & 31)); }
static inline uint32_t TBZ(Register<AArch64, INT> rt, uint8_t bit, uint16_t offset) { return I32(bit & 32 ? 0xb6000000 : 0x36000000 | ((bit & 31) << 19) | ((offset & 0x3fff) << 5) | (rt.value() & 31)); }

/* System instructions */
template <uint8_t op0, uint8_t op1, uint8_t crn, uint8_t crm, uint8_t op2>
static inline uint32_t MRS(Register<AArch64, INT> rt) { return I32(0xd5300000 | (rt.value() & 31) | (op0 == 3 ? 0x80000 : 0) | ((op1 & 7) << 16) | ((crn & 15) << 12) | ((crm & 15) << 8) | ((op2 & 7) << 5)); }
template <uint8_t op0, uint8_t op1, uint8_t crn, uint8_t crm, uint8_t op2>
static inline uint32_t MSR(Register<AArch64, INT> rt) { return I32(0xd5100000 | (rt.value() & 31) | (op0 == 3 ? 0x80000 : 0) | ((op1 & 7) << 16) | ((crn & 15) << 12) | ((crm & 15) << 8) | ((op2 & 7) << 5)); }
static inline uint32_t BRK(uint16_t imm16) { return I32(0xd4200000 | (imm16 << 5)); }
static inline uint32_t HLT(uint16_t imm16) { return I32(0xd4400000 | (imm16 << 5)); }
static inline uint32_t UDF(uint16_t imm16) { return HLT(imm16); }
static inline uint32_t HINT(uint8_t h) { return I32(0xd503201f | ((h & 0x7f) << 5)); }
static inline uint32_t GET_NZCV(Register<AArch64, INT> rt) { return MRS<3,3,4,2,0>(rt); }
static inline uint32_t SET_NZCV(Register<AArch64, INT> rt) { return MSR<3,3,4,2,0>(rt); }
static inline uint32_t CFINV() { return I32(0xd500401f); }

/* Load PC-relatve address */
static inline uint32_t ADR(Register<AArch64, INT> rd, uint32_t imm21) { return I32(0x10000000 | (rd.value() & 31) | ((imm21 & 3) << 29) | (((imm21 >> 2) & 0x7ffff) << 5)); }
static inline uint32_t ADRP(Register<AArch64, INT> rd, uint32_t imm21) { return I32(0x90000000 | (rd.value() & 31) | ((imm21 & 3) << 29) | (((imm21 >> 2) & 0x7ffff) << 5)); }

/* Load/Store instructions */
template< typename width=W32 >
static inline uint32_t LDR(Register<AArch64, INT> rd, int32_t offset19) {
    if (std::is_same<width, W32>::value) { return I32(0x18000000 | ((offset19 & 0x7ffff) << 5) | (rd.value() & 31)); }
    else if (std::is_same<width, X64>::value) { return I32(0x58000000 | ((offset19 & 0x7ffff) << 5) | (rd.value() & 31)); }
    else { return I32(0x98000000 | ((offset19 & 0x7ffff) << 5) | (rd.value() & 31)); } }

#if 0

typedef enum { UXTB = 0, UXTH = 1, UXTW = 2, UXTX = 3, SXTB = 4, SXTH = 5, SXTW = 6, SXTX = 7} reg_extend_t;

/* Load/Store with reg offset */
static inline uint32_t ldr_regoffset(uint8_t rn, uint8_t rt, uint8_t rm, reg_extend_t ext, uint8_t lsl2) { return I32(0xb8600800 | (rt & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | (lsl2 ? 0x1000 : 0) | ((ext & 7) << 13)); }
static inline uint32_t ldr64_regoffset(uint8_t rn, uint8_t rt, uint8_t rm, reg_extend_t ext, uint8_t lsl3) { return I32(0xf8600800 | (rt & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | (lsl3 ? 0x1000 : 0) | ((ext & 7) << 13)); }
static inline uint32_t ldrb_regoffset(uint8_t rn, uint8_t rt, uint8_t rm, reg_extend_t ext) { return I32(0x38600800 | (rt & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | ((ext & 7) << 13)); }
static inline uint32_t ldrsb_regoffset(uint8_t rn, uint8_t rt, uint8_t rm, reg_extend_t ext) { return I32(0x38e00800 | (rt & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | ((ext & 7) << 13)); }
static inline uint32_t ldrsb64_regoffset(uint8_t rn, uint8_t rt, uint8_t rm, reg_extend_t ext) { return I32(0x38a00800 | (rt & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | ((ext & 7) << 13)); }
static inline uint32_t ldrh_regoffset(uint8_t rn, uint8_t rt, uint8_t rm, reg_extend_t ext, uint8_t lsl1) { return I32(0x78600800 | (rt & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | (lsl1 ? 0x1000 : 0) | ((ext & 7) << 13)); }
static inline uint32_t ldrsh_regoffset(uint8_t rn, uint8_t rt, uint8_t rm, reg_extend_t ext, uint8_t lsl1) { return I32(0x78e00800 | (rt & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | (lsl1 ? 0x1000 : 0) | ((ext & 7) << 13)); }
static inline uint32_t ldrsh64_regoffset(uint8_t rn, uint8_t rt, uint8_t rm, reg_extend_t ext, uint8_t lsl1) { return I32(0x78a00800 | (rt & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | (lsl1 ? 0x1000 : 0) | ((ext & 7) << 13)); }
static inline uint32_t ldrsw_regoffset(uint8_t rn, uint8_t rt, uint8_t rm, reg_extend_t ext, uint8_t lsl2) { return I32(0xb8a00800 | (rt & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | (lsl2 ? 0x1000 : 0) | ((ext & 7) << 13)); }
static inline uint32_t str_regoffset(uint8_t rn, uint8_t rt, uint8_t rm, reg_extend_t ext, uint8_t lsl2) { return I32(0xb8200800 | (rt & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | (lsl2 ? 0x1000 : 0) | ((ext & 7) << 13)); }
static inline uint32_t str64_regoffset(uint8_t rn, uint8_t rt, uint8_t rm, reg_extend_t ext, uint8_t lsl3) { return I32(0xf8200800 | (rt & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | (lsl3 ? 0x1000 : 0) | ((ext & 7) << 13)); }
static inline uint32_t strb_regoffset(uint8_t rn, uint8_t rt, uint8_t rm, reg_extend_t ext) { return I32(0x38200800 | (rt & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | ((ext & 7) << 13)); }
static inline uint32_t strh_regoffset(uint8_t rn, uint8_t rt, uint8_t rm, reg_extend_t ext, uint8_t lsl1) { return I32(0x78200800 | (rt & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | (lsl1 ? 0x1000 : 0) | ((ext & 7) << 13)); }

/* Load/Store with scaled offset */
static inline uint32_t ldr_offset(uint8_t rn, uint8_t rt, uint16_t offset14) { return I32(0xb9400000 | (rt & 31) | ((rn & 31) << 5) | (((offset14 >> 2) & 0xfff) << 10)); }
static inline uint32_t ldr_offset_postindex(uint8_t rn, uint8_t rt, int16_t offset9) { return I32(0xb8400400 | (rt & 31) | ((rn & 31) << 5) | ((offset9 & 0x1ff) << 12)); }
static inline uint32_t ldr_offset_preindex(uint8_t rn, uint8_t rt, int16_t offset9) { return I32(0xb8400c00 | (rt & 31) | ((rn & 31) << 5) | ((offset9 & 0x1ff) << 12)); }
static inline uint32_t ldr64_offset(uint8_t rn, uint8_t rt, uint16_t offset15) { return I32(0xf9400000 | (rt & 31) | ((rn & 31) << 5) | (((offset15 >> 3) & 0xfff) << 10)); }
static inline uint32_t ldr64_offset_postindex(uint8_t rn, uint8_t rt, int16_t offset9) { return I32(0xf8400400 | (rt & 31) | ((rn & 31) << 5) | ((offset9 & 0x1ff) << 12)); }
static inline uint32_t ldr64_offset_preindex(uint8_t rn, uint8_t rt, int16_t offset9) { return I32(0xf8400c00 | (rt & 31) | ((rn & 31) << 5) | ((offset9 & 0x1ff) << 12)); }
static inline uint32_t ldrb_offset(uint8_t rn, uint8_t rt, uint16_t offset12) { return I32(0x39400000 | (rt & 31) | ((rn & 31) << 5) | ((offset12 & 0xfff) << 10)); }
static inline uint32_t ldrb_offset_postindex(uint8_t rn, uint8_t rt, int16_t offset9) { return I32(0x38400400 | (rt & 31) | ((rn & 31) << 5) | ((offset9 & 0x1ff) << 12)); }
static inline uint32_t ldrb_offset_preindex(uint8_t rn, uint8_t rt, int16_t offset9) { return I32(0x38400c00 | (rt & 31) | ((rn & 31) << 5) | ((offset9 & 0x1ff) << 12)); }
static inline uint32_t ldrsb_offset(uint8_t rn, uint8_t rt, uint16_t offset12) { return I32(0x39c00000 | (rt & 31) | ((rn & 31) << 5) | ((offset12 & 0xfff) << 10)); }
static inline uint32_t ldrsb_offset_postindex(uint8_t rn, uint8_t rt, int16_t offset9) { return I32(0x38c00400 | (rt & 31) | ((rn & 31) << 5) | ((offset9 & 0x1ff) << 12)); }
static inline uint32_t ldrsb_offset_preindex(uint8_t rn, uint8_t rt, int16_t offset9) { return I32(0x38c00c00 | (rt & 31) | ((rn & 31) << 5) | ((offset9 & 0x1ff) << 12)); }
static inline uint32_t ldrsb64_offset(uint8_t rn, uint8_t rt, uint16_t offset12) { return I32(0x39800000 | (rt & 31) | ((rn & 31) << 5) | ((offset12 & 0xfff) << 10)); }
static inline uint32_t ldrsb64_offset_postindex(uint8_t rn, uint8_t rt, int16_t offset9) { return I32(0x38800400 | (rt & 31) | ((rn & 31) << 5) | ((offset9 & 0x1ff) << 12)); }
static inline uint32_t ldrsb64_offset_preindex(uint8_t rn, uint8_t rt, int16_t offset9) { return I32(0x38800c00 | (rt & 31) | ((rn & 31) << 5) | ((offset9 & 0x1ff) << 12)); }
static inline uint32_t ldrh_offset(uint8_t rn, uint8_t rt, uint16_t offset13) { return I32(0x79400000 | (rt & 31) | ((rn & 31) << 5) | (((offset13 >> 1) & 0xfff) << 10)); }
static inline uint32_t ldrh_offset_postindex(uint8_t rn, uint8_t rt, int16_t offset9) { return I32(0x78400400 | (rt & 31) | ((rn & 31) << 5) | ((offset9 & 0x1ff) << 12)); }
static inline uint32_t ldrh_offset_preindex(uint8_t rn, uint8_t rt, int16_t offset9) { return I32(0x78400c00 | (rt & 31) | ((rn & 31) << 5) | ((offset9 & 0x1ff) << 12)); }
static inline uint32_t ldrsh_offset(uint8_t rn, uint8_t rt, uint16_t offset13) { return I32(0x79c00000 | (rt & 31) | ((rn & 31) << 5) | (((offset13 >> 1) & 0xfff) << 10)); }
static inline uint32_t ldrsh_offset_postindex(uint8_t rn, uint8_t rt, int16_t offset9) { return I32(0x78c00400 | (rt & 31) | ((rn & 31) << 5) | ((offset9 & 0x1ff) << 12)); }
static inline uint32_t ldrsh_offset_preindex(uint8_t rn, uint8_t rt, int16_t offset9) { return I32(0x78c00c00 | (rt & 31) | ((rn & 31) << 5) | ((offset9 & 0x1ff) << 12)); }
static inline uint32_t ldrsh64_offset(uint8_t rn, uint8_t rt, uint16_t offset13) { return I32(0x79800000 | (rt & 31) | ((rn & 31) << 5) | (((offset13 >> 1) & 0xfff) << 10)); }
static inline uint32_t ldrsh64_offset_postindex(uint8_t rn, uint8_t rt, int16_t offset9) { return I32(0x78800400 | (rt & 31) | ((rn & 31) << 5) | ((offset9 & 0x1ff) << 12)); }
static inline uint32_t ldrsh64_offset_preindex(uint8_t rn, uint8_t rt, int16_t offset9) { return I32(0x78800c00 | (rt & 31) | ((rn & 31) << 5) | ((offset9 & 0x1ff) << 12)); }
static inline uint32_t ldrsw_offset(uint8_t rn, uint8_t rt, uint16_t offset14) { return I32(0xb9800000 | (rt & 31) | ((rn & 31) << 5) | (((offset14 >> 2) & 0xfff) << 10)); }
static inline uint32_t ldrsw_offset_postindex(uint8_t rn, uint8_t rt, int16_t offset9) { return I32(0xb8800400 | (rt & 31) | ((rn & 31) << 5) | ((offset9 & 0x1ff) << 12)); }
static inline uint32_t ldrsw_offset_preindex(uint8_t rn, uint8_t rt, int16_t offset9) { return I32(0xb8800c00 | (rt & 31) | ((rn & 31) << 5) | ((offset9 & 0x1ff) << 12)); }
static inline uint32_t str_offset(uint8_t rn, uint8_t rt, uint16_t offset14) { return I32(0xb9000000 | (rt & 31) | ((rn & 31) << 5) | (((offset14 >> 2) & 0xfff) << 10)); }
static inline uint32_t str_offset_postindex(uint8_t rn, uint8_t rt, int16_t offset9) { return I32(0xb8000400 | (rt & 31) | ((rn & 31) << 5) | ((offset9 & 0x1ff) << 12)); }
static inline uint32_t str_offset_preindex(uint8_t rn, uint8_t rt, int16_t offset9) { return I32(0xb8000c00 | (rt & 31) | ((rn & 31) << 5) | ((offset9 & 0x1ff) << 12)); }
static inline uint32_t str64_offset(uint8_t rn, uint8_t rt, uint16_t offset15) { return I32(0xf9000000 | (rt & 31) | ((rn & 31) << 5) | (((offset15 >> 3) & 0xfff) << 10)); }
static inline uint32_t str64_offset_postindex(uint8_t rn, uint8_t rt, int16_t offset9) { return I32(0xf8000400 | (rt & 31) | ((rn & 31) << 5) | ((offset9 & 0x1ff) << 12)); }
static inline uint32_t str64_offset_preindex(uint8_t rn, uint8_t rt, int16_t offset9) { return I32(0xf8000c00 | (rt & 31) | ((rn & 31) << 5) | ((offset9 & 0x1ff) << 12)); }
static inline uint32_t strh_offset(uint8_t rn, uint8_t rt, uint16_t offset13) { return I32(0x79000000 | (rt & 31) | ((rn & 31) << 5) | (((offset13 >> 1) & 0xfff) << 10)); }
static inline uint32_t strh_offset_postindex(uint8_t rn, uint8_t rt, int16_t offset9) { return I32(0x78000400 | (rt & 31) | ((rn & 31) << 5) | ((offset9 & 0x1ff) << 12)); }
static inline uint32_t strh_offset_preindex(uint8_t rn, uint8_t rt, int16_t offset9) { return I32(0x78000c00 | (rt & 31) | ((rn & 31) << 5) | ((offset9 & 0x1ff) << 12)); }
static inline uint32_t strb_offset(uint8_t rn, uint8_t rt, uint16_t offset13) { return I32(0x39000000 | (rt & 31) | ((rn & 31) << 5) | (((offset13 >> 1) & 0xfff) << 10)); }
static inline uint32_t strb_offset_postindex(uint8_t rn, uint8_t rt, int16_t offset9) { return I32(0x38000400 | (rt & 31) | ((rn & 31) << 5) | ((offset9 & 0x1ff) << 12)); }
static inline uint32_t strb_offset_preindex(uint8_t rn, uint8_t rt, int16_t offset9) { return I32(0x38000c00 | (rt & 31) | ((rn & 31) << 5) | ((offset9 & 0x1ff) << 12)); }

/* Load/Store pair */
static inline uint32_t ldp(uint8_t rn, uint8_t rt1, uint8_t rt2, int8_t imm) { return I32(0x29400000 | (rt1 & 31) | ((rt2 & 31) << 10) | ((rn & 31) << 5) | (((imm / 4) & 0x7f) << 15)); }
static inline uint32_t ldp64(uint8_t rn, uint8_t rt1, uint8_t rt2, int8_t imm) { return I32(0xa9400000 | (rt1 & 31) | ((rt2 & 31) << 10) | ((rn & 31) << 5) | (((imm / 8) & 0x7f) << 15)); }
static inline uint32_t ldp_postindex(uint8_t rn, uint8_t rt1, uint8_t rt2, int8_t imm) { return I32(0x28c00000 | (rt1 & 31) | ((rt2 & 31) << 10) | ((rn & 31) << 5) | (((imm / 4) & 0x7f) << 15)); }
static inline uint32_t ldp64_postindex(uint8_t rn, uint8_t rt1, uint8_t rt2, int8_t imm) { return I32(0xa8c00000 | (rt1 & 31) | ((rt2 & 31) << 10) | ((rn & 31) << 5) | (((imm / 8) & 0x7f) << 15)); }
static inline uint32_t ldp_preindex(uint8_t rn, uint8_t rt1, uint8_t rt2, int8_t imm) { return I32(0x29c00000 | (rt1 & 31) | ((rt2 & 31) << 10) | ((rn & 31) << 5) | (((imm / 4) & 0x7f) << 15)); }
static inline uint32_t ldp64_preindex(uint8_t rn, uint8_t rt1, uint8_t rt2, int8_t imm) { return I32(0xa9c00000 | (rt1 & 31) | ((rt2 & 31) << 10) | ((rn & 31) << 5) | (((imm / 8) & 0x7f) << 15)); }
static inline uint32_t stp(uint8_t rn, uint8_t rt1, uint8_t rt2, int8_t imm) { return I32(0x29000000 | (rt1 & 31) | ((rt2 & 31) << 10) | ((rn & 31) << 5) | (((imm / 4) & 0x7f) << 15)); }
static inline uint32_t stp64(uint8_t rn, uint8_t rt1, uint8_t rt2, int8_t imm) { return I32(0xa9000000 | (rt1 & 31) | ((rt2 & 31) << 10) | ((rn & 31) << 5) | (((imm / 8) & 0x7f) << 15)); }
static inline uint32_t stp_postindex(uint8_t rn, uint8_t rt1, uint8_t rt2, int8_t imm) { return I32(0x28800000 | (rt1 & 31) | ((rt2 & 31) << 10) | ((rn & 31) << 5) | (((imm / 4) & 0x7f) << 15)); }
static inline uint32_t stp64_postindex(uint8_t rn, uint8_t rt1, uint8_t rt2, int8_t imm) { return I32(0xa8800000 | (rt1 & 31) | ((rt2 & 31) << 10) | ((rn & 31) << 5) | (((imm / 8) & 0x7f) << 15)); }
static inline uint32_t stp_preindex(uint8_t rn, uint8_t rt1, uint8_t rt2, int8_t imm) { return I32(0x29800000 | (rt1 & 31) | ((rt2 & 31) << 10) | ((rn & 31) << 5) | (((imm / 4) & 0x7f) << 15)); }
static inline uint32_t stp64_preindex(uint8_t rn, uint8_t rt1, uint8_t rt2, int8_t imm) { return I32(0xa9800000 | (rt1 & 31) | ((rt2 & 31) << 10) | ((rn & 31) << 5) | (((imm / 8) & 0x7f) << 15)); }

/* Load/Store exclusive */
static inline uint32_t ldxr(uint8_t rt, uint8_t rn) { return I32(0x885f7c00 | (rt & 31) | ((rn & 31) << 5)); }
static inline uint32_t ldxr64(uint8_t rt, uint8_t rn) { return I32(0xc85f7c00 | (rt & 31) | ((rn & 31) << 5)); }
static inline uint32_t ldxrb(uint8_t rt, uint8_t rn) { return I32(0x085f7c00 | (rt & 31) | ((rn & 31) << 5)); }
static inline uint32_t ldxrh(uint8_t rt, uint8_t rn) { return I32(0x485f7c00 | (rt & 31) | ((rn & 31) << 5)); }
static inline uint32_t stxr(uint8_t rt, uint8_t rn, uint8_t rs) { return I32(0x88007c00 | (rt & 31) | ((rn & 31) << 5) | ((rs & 31) << 16)); }
static inline uint32_t stxr64(uint8_t rt, uint8_t rn, uint8_t rs) { return I32(0xc8007c00 | (rt & 31) | ((rn & 31) << 5) | ((rs & 31) << 16)); }
static inline uint32_t stxrb(uint8_t rt, uint8_t rn, uint8_t rs) { return I32(0x08007c00 | (rt & 31) | ((rn & 31) << 5) | ((rs & 31) << 16)); }
static inline uint32_t stxrh(uint8_t rt, uint8_t rn, uint8_t rs) { return I32(0x48007c00 | (rt & 31) | ((rn & 31) << 5) | ((rs & 31) << 16)); }

/* Load/Store with unscaled offset */
static inline uint32_t ldur_offset(uint8_t rn, uint8_t rt, int16_t offset9) { return I32(0xb8400000 | (rt & 31) | ((rn & 31) << 5) | ((offset9 & 0x1ff) << 12)); }
static inline uint32_t ldur64_offset(uint8_t rn, uint8_t rt, int16_t offset9) { return I32(0xf8400000 | (rt & 31) | ((rn & 31) << 5) | ((offset9 & 0x1ff) << 12)); }
static inline uint32_t ldurb_offset(uint8_t rn, uint8_t rt, int16_t offset9) { return I32(0x38400000 | (rt & 31) | ((rn & 31) << 5) | ((offset9 & 0x1ff) << 12)); }
static inline uint32_t ldursb_offset(uint8_t rn, uint8_t rt, int16_t offset9) { return I32(0x38c00000 | (rt & 31) | ((rn & 31) << 5) | ((offset9 & 0x1ff) << 12)); }
static inline uint32_t ldursb64_offset(uint8_t rn, uint8_t rt, int16_t offset9) { return I32(0x38800000 | (rt & 31) | ((rn & 31) << 5) | ((offset9 & 0x1ff) << 12)); }
static inline uint32_t ldurh_offset(uint8_t rn, uint8_t rt, int16_t offset9) { return I32(0x78400000 | (rt & 31) | ((rn & 31) << 5) | ((offset9 & 0x1ff) << 12)); }
static inline uint32_t ldursh_offset(uint8_t rn, uint8_t rt, int16_t offset9) { return I32(0x78c00000 | (rt & 31) | ((rn & 31) << 5) | ((offset9 & 0x1ff) << 12)); }
static inline uint32_t ldursh64_offset(uint8_t rn, uint8_t rt, int16_t offset9) { return I32(0x78800000 | (rt & 31) | ((rn & 31) << 5) | ((offset9 & 0x1ff) << 12)); }
static inline uint32_t ldursw_offset(uint8_t rn, uint8_t rt, int16_t offset9) { return I32(0xb8800000 | (rt & 31) | ((rn & 31) << 5) | ((offset9 & 0x1ff) << 12)); }
static inline uint32_t stur_offset(uint8_t rn, uint8_t rt, int16_t offset9) { return I32(0xb8000000 | (rt & 31) | ((rn & 31) << 5) | ((offset9 & 0x1ff) << 12)); }
static inline uint32_t stur64_offset(uint8_t rn, uint8_t rt, int16_t offset9) { return I32(0xf8000000 | (rt & 31) | ((rn & 31) << 5) | ((offset9 & 0x1ff) << 12)); }
static inline uint32_t sturb_offset(uint8_t rn, uint8_t rt, int16_t offset9) { return I32(0x38000000 | (rt & 31) | ((rn & 31) << 5) | ((offset9 & 0x1ff) << 12)); }
static inline uint32_t sturh_offset(uint8_t rn, uint8_t rt, int16_t offset9) { return I32(0x78000000 | (rt & 31) | ((rn & 31) << 5) | ((offset9 & 0x1ff) << 12)); }

/* Data processing: immediate */
static inline uint32_t add_immed(uint8_t rd, uint8_t rn, uint16_t imm12) { return I32(0x11000000 | ((imm12 & 0xfff) << 10) | ((rn & 31) << 5) | (rd & 31)); }
static inline uint32_t add64_immed(uint8_t rd, uint8_t rn, uint16_t imm12) { return I32(0x91000000 | ((imm12 & 0xfff) << 10) | ((rn & 31) << 5) | (rd & 31)); }
static inline uint32_t adds_immed(uint8_t rd, uint8_t rn, uint16_t imm12) { return I32(0x31000000 | ((imm12 & 0xfff) << 10) | ((rn & 31) << 5) | (rd & 31)); }
static inline uint32_t adds64_immed(uint8_t rd, uint8_t rn, uint16_t imm12) { return I32(0xb1000000 | ((imm12 & 0xfff) << 10) | ((rn & 31) << 5) | (rd & 31)); }
static inline uint32_t sub_immed(uint8_t rd, uint8_t rn, uint16_t imm12) { return I32(0x51000000 | ((imm12 & 0xfff) << 10) | ((rn & 31) << 5) | (rd & 31)); }
static inline uint32_t sub64_immed(uint8_t rd, uint8_t rn, uint16_t imm12) { return I32(0xd1000000 | ((imm12 & 0xfff) << 10) | ((rn & 31) << 5) | (rd & 31)); }
static inline uint32_t subs_immed(uint8_t rd, uint8_t rn, uint16_t imm12) { return I32(0x71000000 | ((imm12 & 0xfff) << 10) | ((rn & 31) << 5) | (rd & 31)); }
static inline uint32_t subs64_immed(uint8_t rd, uint8_t rn, uint16_t imm12) { return I32(0xf1000000 | ((imm12 & 0xfff) << 10) | ((rn & 31) << 5) | (rd & 31)); }
static inline uint32_t cmp_immed(uint8_t rn, uint16_t imm12) { return subs_immed(31, rn, imm12); }
static inline uint32_t cmp64_immed(uint8_t rn, uint16_t imm12) { return subs64_immed(31, rn, imm12); }
static inline uint32_t cmn_immed(uint8_t rn, uint16_t imm12) { return adds_immed(31, rn, imm12); }
static inline uint32_t cmn64_immed(uint8_t rn, uint16_t imm12) { return adds64_immed(31, rn, imm12); }
static inline uint32_t add_immed_lsl12(uint8_t rd, uint8_t rn, uint16_t imm12) { return I32(0x11000000 | (1 << 22) | ((imm12 & 0xfff) << 10) | ((rn & 31) << 5) | (rd & 31)); }
static inline uint32_t add64_immed_lsl12(uint8_t rd, uint8_t rn, uint16_t imm12) { return I32(0x91000000 | (1 << 22) | ((imm12 & 0xfff) << 10) | ((rn & 31) << 5) | (rd & 31)); }
static inline uint32_t adds_immed_lsl12(uint8_t rd, uint8_t rn, uint16_t imm12) { return I32(0x31000000 | (1 << 22) | ((imm12 & 0xfff) << 10) | ((rn & 31) << 5) | (rd & 31)); }
static inline uint32_t adds64_immed_lsl12(uint8_t rd, uint8_t rn, uint16_t imm12) { return I32(0xb1000000 | (1 << 22) | ((imm12 & 0xfff) << 10) | ((rn & 31) << 5) | (rd & 31)); }
static inline uint32_t sub_immed_lsl12(uint8_t rd, uint8_t rn, uint16_t imm12) { return I32(0x51000000 | (1 << 22) | ((imm12 & 0xfff) << 10) | ((rn & 31) << 5) | (rd & 31)); }
static inline uint32_t sub64_immed_lsl12(uint8_t rd, uint8_t rn, uint16_t imm12) { return I32(0xd1000000 | (1 << 22) | ((imm12 & 0xfff) << 10) | ((rn & 31) << 5) | (rd & 31)); }
static inline uint32_t subs_immed_lsl12(uint8_t rd, uint8_t rn, uint16_t imm12) { return I32(0x71000000 | (1 << 22) | ((imm12 & 0xfff) << 10) | ((rn & 31) << 5) | (rd & 31)); }
static inline uint32_t subs64_immed_lsl12(uint8_t rd, uint8_t rn, uint16_t imm12) { return I32(0xf1000000 | (1 << 22) | ((imm12 & 0xfff) << 10) | ((rn & 31) << 5) | (rd & 31)); }
static inline uint32_t cmp_immed_lsl12(uint8_t rn, uint16_t imm12) { return subs_immed_lsl12(31, rn, imm12); }
static inline uint32_t cmp64_immed_lsl12(uint8_t rn, uint16_t imm12) { return subs64_immed_lsl12(31, rn, imm12); }
static inline uint32_t cmn_immed_lsl12(uint8_t rn, uint16_t imm12) { return adds_immed_lsl12(31, rn, imm12); }
static inline uint32_t cmn64_immed_lsl12(uint8_t rn, uint16_t imm12) { return adds64_immed_lsl12(31, rn, imm12); }
static inline uint32_t and_immed(uint8_t rd, uint8_t rn, uint8_t width, uint8_t ror) { return I32(0x12000000 | (rd & 31) | ((rn & 31) << 5) | (((width - 1) & 0x3f) << 10) | ((ror & 0x3f) << 16)); }
static inline uint32_t and64_immed(uint8_t rd, uint8_t rn, uint8_t width, uint8_t ror, uint8_t n) { return I32(0x92000000 | (n ? (1 << 22) : 0) | (rd & 31) | ((rn & 31) << 5) | (((width - 1) & 0x3f) << 10) | ((ror & 0x3f) << 16)); }
static inline uint32_t bic_immed(uint8_t rd, uint8_t rn, uint8_t width, uint8_t ror) { return and_immed(rd, rn, 32 - width, ror - width); }
static inline uint32_t ands_immed(uint8_t rd, uint8_t rn, uint8_t width, uint8_t ror) { return I32(0x72000000 | (rd & 31) | ((rn & 31) << 5) | (((width - 1) & 0x3f) << 10) | ((ror & 0x3f) << 16)); }
static inline uint32_t ands64_immed(uint8_t rd, uint8_t rn, uint8_t width, uint8_t ror, uint8_t n) { return I32(0xf2000000 | (n ? (1 << 22) : 0) | (rd & 31) | ((rn & 31) << 5) | (((width - 1) & 0x3f) << 10) | ((ror & 0x3f) << 16)); }
static inline uint32_t bics_immed(uint8_t rd, uint8_t rn, uint8_t width, uint8_t ror) { return ands_immed(rd, rn, 32 - width, ror - width); }
static inline uint32_t eor_immed(uint8_t rd, uint8_t rn, uint8_t width, uint8_t ror) { return I32(0x52000000 | (rd & 31) | ((rn & 31) << 5) | (((width - 1) & 0x3f) << 10) | ((ror & 0x3f) << 16)); }
static inline uint32_t eor64_immed(uint8_t rd, uint8_t rn, uint8_t width, uint8_t ror, uint8_t n) { return I32(0xd2000000 | (n ? (1 << 22) : 0) | (rd & 31) | ((rn & 31) << 5) | (((width - 1) & 0x3f) << 10) | ((ror & 0x3f) << 16)); }
static inline uint32_t orr_immed(uint8_t rd, uint8_t rn, uint8_t width, uint8_t ror) { return I32(0x32000000 | (rd & 31) | ((rn & 31) << 5) | (((width - 1) & 0x3f) << 10) | ((ror & 0x3f) << 16)); }
static inline uint32_t orr64_immed(uint8_t rd, uint8_t rn, uint8_t width, uint8_t ror, uint8_t n) { return I32(0xb2000000 | (n ? (1 << 22) : 0) | (rd & 31) | ((rn & 31) << 5) | (((width - 1) & 0x3f) << 10) | ((ror & 0x3f) << 16)); }
static inline uint32_t tst_immed(uint8_t rn, uint8_t width, uint8_t ror) { return ands_immed(31, rn, width, ror); }
static inline uint32_t tst64_immed(uint8_t rn, uint8_t width, uint8_t ror, uint8_t n) { return ands64_immed(31, rn, width, ror, n); }

/* Data processing: bitfields */
static inline uint32_t bfm(uint8_t rd, uint8_t rn, uint8_t immr, uint8_t imms) { return I32(0x33000000 | (rd & 31) | ((rn & 31) << 5) | ((immr & 0x3f) << 16) | ((imms & 0x3f) << 10)); }
static inline uint32_t bfm64(uint8_t rd, uint8_t rn, uint8_t immr, uint8_t imms) { return I32(0xb3400000 | (rd & 31) | ((rn & 31) << 5) | ((immr & 0x3f) << 16) | ((imms & 0x3f) << 10)); }
static inline uint32_t sbfm(uint8_t rd, uint8_t rn, uint8_t immr, uint8_t imms) { return I32(0x13000000 | (rd & 31) | ((rn & 31) << 5) | ((immr & 0x3f) << 16) | ((imms & 0x3f) << 10)); }
static inline uint32_t sbfm64(uint8_t rd, uint8_t rn, uint8_t immr, uint8_t imms) { return I32(0x93400000 | (rd & 31) | ((rn & 31) << 5) | ((immr & 0x3f) << 16) | ((imms & 0x3f) << 10)); }
static inline uint32_t ubfm(uint8_t rd, uint8_t rn, uint8_t immr, uint8_t imms) { return I32(0x53000000 | (rd & 31) | ((rn & 31) << 5) | ((immr & 0x3f) << 16) | ((imms & 0x3f) << 10)); }
static inline uint32_t ubfm64(uint8_t rd, uint8_t rn, uint8_t immr, uint8_t imms) { return I32(0xd3400000 | (rd & 31) | ((rn & 31) << 5) | ((immr & 0x3f) << 16) | ((imms & 0x3f) << 10)); }
static inline uint32_t bfi(uint8_t rd, uint8_t rn, uint8_t lsb, uint8_t width) { return bfm(rd, rn, 31 & (32-lsb), width - 1); }
static inline uint32_t bfi64(uint8_t rd, uint8_t rn, uint8_t lsb, uint8_t width) { return bfm64(rd, rn, 63 & (64-lsb), width - 1); }
static inline uint32_t bfxil(uint8_t rd, uint8_t rn, uint8_t lsb, uint8_t width) { return bfm(rd, rn, lsb, lsb + width - 1); }
static inline uint32_t bfxil64(uint8_t rd, uint8_t rn, uint8_t lsb, uint8_t width) { return bfm64(rd, rn, lsb, lsb + width - 1); }
static inline uint32_t sbfx(uint8_t rd, uint8_t rn, uint8_t lsb, uint8_t width) { return sbfm(rd, rn, lsb, lsb + width - 1); }
static inline uint32_t sbfx64(uint8_t rd, uint8_t rn, uint8_t lsb, uint8_t width) { return sbfm64(rd, rn, lsb, lsb + width - 1); }
static inline uint32_t sbfiz(uint8_t rd, uint8_t rn, uint8_t lsb, uint8_t width) { return sbfm(rd, rn, 31 & (32-lsb), width - 1); }
static inline uint32_t sbfiz64(uint8_t rd, uint8_t rn, uint8_t lsb, uint8_t width) { return sbfm64(rd, rn, 63 & (64-lsb), width - 1); }
static inline uint32_t ubfx(uint8_t rd, uint8_t rn, uint8_t lsb, uint8_t width) { return ubfm(rd, rn, lsb, lsb + width - 1); }
static inline uint32_t ubfx64(uint8_t rd, uint8_t rn, uint8_t lsb, uint8_t width) { return ubfm64(rd, rn, lsb, lsb + width - 1); }
static inline uint32_t ubfiz(uint8_t rd, uint8_t rn, uint8_t lsb, uint8_t width) { return ubfm(rd, rn, 31 & (32-lsb), width - 1); }
static inline uint32_t ubfiz64(uint8_t rd, uint8_t rn, uint8_t lsb, uint8_t width) { return ubfm64(rd, rn, 63 & (64-lsb), width - 1); }

/* Data processing: register extract */
static inline uint32_t extr(uint8_t rd, uint8_t rn, uint8_t rm, uint8_t lsb) { return I32(0x13800000 | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | ((lsb & 63) << 10)); }
static inline uint32_t extr64(uint8_t rd, uint8_t rn, uint8_t rm, uint8_t lsb) { return I32(0x93c00000 | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | ((lsb & 63) << 10)); }

/* Data processing: shift immediate */
static inline uint32_t asr(uint8_t rd, uint8_t rn, uint8_t lsb) { return sbfm(rd, rn, lsb, 31); }
static inline uint32_t asr64(uint8_t rd, uint8_t rn, uint8_t lsb) { return sbfm64(rd, rn, lsb, 63); }
static inline uint32_t lsl(uint8_t rd, uint8_t rn, uint8_t lsb) { return ubfm(rd, rn, 31 & (32 - lsb), 31 - lsb); }
static inline uint32_t lsl64(uint8_t rd, uint8_t rn, uint8_t lsb) { return ubfm64(rd, rn, 63 & (64 - lsb), 63 - lsb); }
static inline uint32_t lsr(uint8_t rd, uint8_t rn, uint8_t lsb) { return ubfm(rd, rn, lsb, 31); }
static inline uint32_t lsr64(uint8_t rd, uint8_t rn, uint8_t lsb) { return ubfm64(rd, rn, lsb, 63); }
static inline uint32_t ror(uint8_t rd, uint8_t rn, uint8_t lsb) { return extr(rd, rn, rn, lsb); }
static inline uint32_t ror64(uint8_t rd, uint8_t rn, uint8_t lsb) { return extr64(rd, rn, rn, lsb); }

/* Data processing: extending */
static inline uint32_t sxtb(uint8_t rd, uint8_t rn) { return sbfm(rd, rn, 0, 7); }
static inline uint32_t sxtb64(uint8_t rd, uint8_t rn) { return sbfm64(rd, rn, 0, 7); }
static inline uint32_t sxth(uint8_t rd, uint8_t rn) { return sbfm(rd, rn, 0, 15); }
static inline uint32_t sxth64(uint8_t rd, uint8_t rn) { return sbfm64(rd, rn, 0, 15); }
static inline uint32_t sxtw64(uint8_t rd, uint8_t rn) { return sbfm64(rd, rn, 0, 31); }
static inline uint32_t uxtb(uint8_t rd, uint8_t rn) { return ubfm(rd, rn, 0, 7); }
static inline uint32_t uxtb64(uint8_t rd, uint8_t rn) { return ubfm64(rd, rn, 0, 7); }
static inline uint32_t uxth(uint8_t rd, uint8_t rn) { return ubfm(rd, rn, 0, 15); }
static inline uint32_t uxth64(uint8_t rd, uint8_t rn) { return ubfm64(rd, rn, 0, 15); }

/* Data processing: move */
static inline uint32_t mov_immed_u16(uint8_t reg, uint16_t val, uint8_t shift16) { return I32(0x52800000 | ((shift16 & 3) << 21) | (val << 5) | (reg & 31)); }
static inline uint32_t mov64_immed_u16(uint8_t reg, uint16_t val, uint8_t shift16) { return I32(0xd2800000 | ((shift16 & 3) << 21) | (val << 5) | (reg & 31)); }
static inline uint32_t movk_immed_u16(uint8_t reg, uint16_t val, uint8_t shift16) { return I32(0x72800000 | ((shift16 & 3) << 21) | (val << 5) | (reg & 31)); }
static inline uint32_t movk64_immed_u16(uint8_t reg, uint16_t val, uint8_t shift16) { return I32(0xf2800000 | ((shift16 & 3) << 21) | (val << 5) | (reg & 31)); }
static inline uint32_t movn_immed_u16(uint8_t reg, uint16_t val, uint8_t shift16) { return I32(0x12800000 | ((shift16 & 3) << 21) | (val << 5) | (reg & 31)); }
static inline uint32_t movn64_immed_u16(uint8_t reg, uint16_t val, uint8_t shift16) { return I32(0x92800000 | ((shift16 & 3) << 21) | (val << 5) | (reg & 31)); }
static inline uint32_t movw_immed_u16(uint8_t reg, uint16_t val) { return mov_immed_u16(reg, val, 0); }
static inline uint32_t movt_immed_u16(uint8_t reg, uint16_t val) { return movk_immed_u16(reg, val, 1); }
static inline uint32_t mov_immed_s8(uint8_t reg, int8_t val) { if (val < 0) return movn_immed_u16(reg, -val - 1, 0); else return mov_immed_u16(reg, val, 0); }
static inline uint32_t mov_immed_u8(uint8_t reg, uint8_t val) { return mov_immed_u16(reg, val, 0); }

typedef enum { LSL = 0, LSR = 1, ASR = 2, ROR = 3 } shift_t;
/* Data processing: register */
static inline uint32_t add_reg(uint8_t rd, uint8_t rn, uint8_t rm, shift_t shift, uint8_t amount) { return I32(0x0b000000 | (shift << 22) | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | ((amount & 63) << 10)); }
static inline uint32_t add64_reg(uint8_t rd, uint8_t rn, uint8_t rm, shift_t shift, uint8_t amount) { return I32(0x8b000000 | (shift << 22) | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | ((amount & 63) << 10)); }
static inline uint32_t adds_reg(uint8_t rd, uint8_t rn, uint8_t rm, shift_t shift, uint8_t amount) { return I32(0x2b000000 | (shift << 22) | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | ((amount & 63) << 10)); }
static inline uint32_t adds64_reg(uint8_t rd, uint8_t rn, uint8_t rm, shift_t shift, uint8_t amount) { return I32(0xab000000 | (shift << 22) | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | ((amount & 63) << 10)); }
static inline uint32_t sub_reg(uint8_t rd, uint8_t rn, uint8_t rm, shift_t shift, uint8_t amount) { return I32(0x4b000000 | (shift << 22) | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | ((amount & 63) << 10)); }
static inline uint32_t sub64_reg(uint8_t rd, uint8_t rn, uint8_t rm, shift_t shift, uint8_t amount) { return I32(0xcb000000 | (shift << 22) | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | ((amount & 63) << 10)); }
static inline uint32_t subs_reg(uint8_t rd, uint8_t rn, uint8_t rm, shift_t shift, uint8_t amount) { return I32(0x6b000000 | (shift << 22) | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | ((amount & 63) << 10)); }
static inline uint32_t subs64_reg(uint8_t rd, uint8_t rn, uint8_t rm, shift_t shift, uint8_t amount) { return I32(0xeb000000 | (shift << 22) | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | ((amount & 63) << 10)); }
static inline uint32_t cmn_reg(uint8_t rn, uint8_t rm, shift_t shift, uint8_t amount) { return adds_reg(31, rn, rm, shift, amount); }
static inline uint32_t cmn64_reg(uint8_t rn, uint8_t rm, shift_t shift, uint8_t amount) { return adds64_reg(31, rn, rm, shift, amount); }
static inline uint32_t cmp_reg(uint8_t rn, uint8_t rm, shift_t shift, uint8_t amount) { return subs_reg(31, rn, rm, shift, amount); }
static inline uint32_t cmp64_reg(uint8_t rn, uint8_t rm, shift_t shift, uint8_t amount) { return subs64_reg(31, rn, rm, shift, amount); }
static inline uint32_t neg_reg(uint8_t rd, uint8_t rm, shift_t shift, uint8_t amount) { return sub_reg(rd, 31, rm, shift, amount); }
static inline uint32_t neg64_reg(uint8_t rd, uint8_t rm, shift_t shift, uint8_t amount) { return sub64_reg(rd, 31, rm, shift, amount); }
static inline uint32_t negs_reg(uint8_t rd, uint8_t rm, shift_t shift, uint8_t amount) { return subs_reg(rd, 31, rm, shift, amount); }
static inline uint32_t negs64_reg(uint8_t rd, uint8_t rm, shift_t shift, uint8_t amount) { return subs64_reg(rd, 31, rm, shift, amount); }

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

/* Data prcessing: arithmetic with carry */
static inline uint32_t adc(uint8_t rd, uint8_t rn, uint8_t rm) { return I32(0x1a000000 | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16)); }
static inline uint32_t adc64(uint8_t rd, uint8_t rn, uint8_t rm) { return I32(0x9a000000 | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16)); }
static inline uint32_t adcs(uint8_t rd, uint8_t rn, uint8_t rm) { return I32(0x3a000000 | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16)); }
static inline uint32_t adcs64(uint8_t rd, uint8_t rn, uint8_t rm) { return I32(0xba000000 | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16)); }
static inline uint32_t sbc(uint8_t rd, uint8_t rn, uint8_t rm) { return I32(0x5a000000 | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16)); }
static inline uint32_t sbc64(uint8_t rd, uint8_t rn, uint8_t rm) { return I32(0xda000000 | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16)); }
static inline uint32_t sbcs(uint8_t rd, uint8_t rn, uint8_t rm) { return I32(0x7a000000 | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16)); }
static inline uint32_t sbcs64(uint8_t rd, uint8_t rn, uint8_t rm) { return I32(0xfa000000 | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16)); }
static inline uint32_t ngc(uint8_t rd, uint8_t rm) { return sbc(rd, 31, rm); }
static inline uint32_t ngc64(uint8_t rd, uint8_t rm) { return sbc64(rd, 31, rm); }
static inline uint32_t ngcs(uint8_t rd, uint8_t rm) { return sbcs(rd, 31, rm); }
static inline uint32_t ngcs64(uint8_t rd, uint8_t rm) { return sbcs64(rd, 31, rm); }

/* Data processing: conditional select */

static inline uint32_t csel(uint8_t rd, uint8_t rn, uint8_t rm, uint8_t cond) { return I32(0x1a800000 | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | ((cond & 15) << 12)); }
static inline uint32_t csel64(uint8_t rd, uint8_t rn, uint8_t rm, uint8_t cond) { return I32(0x9a800000 | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 6) | ((cond & 15) << 12)); }
static inline uint32_t csinc(uint8_t rd, uint8_t rn, uint8_t rm, uint8_t cond) { return I32(0x1a800400 | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | ((cond & 15) << 12)); }
static inline uint32_t csinc64(uint8_t rd, uint8_t rn, uint8_t rm, uint8_t cond) { return I32(0x9a800400 | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | ((cond & 15) << 12)); }
static inline uint32_t csinv(uint8_t rd, uint8_t rn, uint8_t rm, uint8_t cond) { return I32(0x5a800000 | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | ((cond & 15) << 12)); }
static inline uint32_t csinv64(uint8_t rd, uint8_t rn, uint8_t rm, uint8_t cond) { return I32(0xda800000 | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | ((cond & 15) << 12)); }
static inline uint32_t csetm(uint8_t rd, uint8_t cond) { return csinv(rd, 31, 31, cond ^ 1); }
static inline uint32_t csetm64(uint8_t rd, uint8_t cond) { return csinv64(rd, 31, 31, cond ^ 1); }
static inline uint32_t cset(uint8_t rd, uint8_t cond) { return csinc(rd, 31, 31, cond ^ 1); }
static inline uint32_t cset64(uint8_t rd, uint8_t cond) { return csinc64(rd, 31, 31, cond ^ 1); }

/* Data processing: logic */
static inline uint32_t and_reg(uint8_t rd, uint8_t rn, uint8_t rm, shift_t shift, uint8_t amount) { return I32(0x0a000000 | (shift << 22) | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | ((amount & 63) << 10)); }
static inline uint32_t and64_reg(uint8_t rd, uint8_t rn, uint8_t rm, shift_t shift, uint8_t amount) { return I32(0x8a000000 | (shift << 22) | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | ((amount & 63) << 10)); }
static inline uint32_t ands_reg(uint8_t rd, uint8_t rn, uint8_t rm, shift_t shift, uint8_t amount) { return I32(0x6a000000 | (shift << 22) | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | ((amount & 63) << 10)); }
static inline uint32_t ands64_reg(uint8_t rd, uint8_t rn, uint8_t rm, shift_t shift, uint8_t amount) { return I32(0xea000000 | (shift << 22) | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | ((amount & 63) << 10)); }
static inline uint32_t bic_reg(uint8_t rd, uint8_t rn, uint8_t rm, shift_t shift, uint8_t amount) { return I32(0x0a200000 | (shift << 22) | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | ((amount & 63) << 10)); }
static inline uint32_t bic64_reg(uint8_t rd, uint8_t rn, uint8_t rm, shift_t shift, uint8_t amount) { return I32(0x8a200000 | (shift << 22) | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | ((amount & 63) << 10)); }
static inline uint32_t bics_reg(uint8_t rd, uint8_t rn, uint8_t rm, shift_t shift, uint8_t amount) { return I32(0x6a200000 | (shift << 22) | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | ((amount & 63) << 10)); }
static inline uint32_t bics64_reg(uint8_t rd, uint8_t rn, uint8_t rm, shift_t shift, uint8_t amount) { return I32(0xea200000 | (shift << 22) | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | ((amount & 63) << 10)); }
static inline uint32_t eon_reg(uint8_t rd, uint8_t rn, uint8_t rm, shift_t shift, uint8_t amount) { return I32(0x4a200000 | (shift << 22) | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | ((amount & 63) << 10)); }
static inline uint32_t eon64_reg(uint8_t rd, uint8_t rn, uint8_t rm, shift_t shift, uint8_t amount) { return I32(0xca200000 | (shift << 22) | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | ((amount & 63) << 10)); }
static inline uint32_t eor_reg(uint8_t rd, uint8_t rn, uint8_t rm, shift_t shift, uint8_t amount) { return I32(0x4a000000 | (shift << 22) | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | ((amount & 63) << 10)); }
static inline uint32_t eor64_reg(uint8_t rd, uint8_t rn, uint8_t rm, shift_t shift, uint8_t amount) { return I32(0xca000000 | (shift << 22) | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | ((amount & 63) << 10)); }
static inline uint32_t orr_reg(uint8_t rd, uint8_t rn, uint8_t rm, shift_t shift, uint8_t amount) { return I32(0x2a000000 | (shift << 22) | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | ((amount & 63) << 10)); }
static inline uint32_t orr64_reg(uint8_t rd, uint8_t rn, uint8_t rm, shift_t shift, uint8_t amount) { return I32(0xaa000000 | (shift << 22) | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | ((amount & 63) << 10)); }
static inline uint32_t orn_reg(uint8_t rd, uint8_t rn, uint8_t rm, shift_t shift, uint8_t amount) { return I32(0x2a200000 | (shift << 22) | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | ((amount & 63) << 10)); }
static inline uint32_t orn64_reg(uint8_t rd, uint8_t rn, uint8_t rm, shift_t shift, uint8_t amount) { return I32(0xaa200000 | (shift << 22) | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16) | ((amount & 63) << 10)); }
static inline uint32_t mvn_reg(uint8_t rd, uint8_t rm, shift_t shift, uint8_t amount) { return orn_reg(rd, 31, rm, shift, amount); }
static inline uint32_t mvn64_reg(uint8_t rd, uint8_t rm, shift_t shift, uint8_t amount) { return orn64_reg(rd, 31, rm, shift, amount); }
static inline uint32_t tst_reg(uint8_t rn, uint8_t rm, shift_t shift, uint8_t amount) { return ands_reg(31, rn, rm, shift, amount); }
static inline uint32_t tst64_reg(uint8_t rn, uint8_t rm, shift_t shift, uint8_t amount) { return ands64_reg(31, rn, rm, shift, amount); }

/* Data processing: move register */
static inline uint32_t mov_reg(uint8_t rd, uint8_t rm) { return orr_reg(rd, 31, rm, LSL, 0); }
static inline uint32_t mov64_reg(uint8_t rd, uint8_t rm) { return orr64_reg(rd, 31, rm, LSL, 0); }

/* Data processing: shift reg */
static inline uint32_t asrv(uint8_t rd, uint8_t rn, uint8_t rm) { return I32(0x1ac02800 | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16)); }
static inline uint32_t asrv64(uint8_t rd, uint8_t rn, uint8_t rm) { return I32(0x9ac02800 | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16)); }
static inline uint32_t lslv(uint8_t rd, uint8_t rn, uint8_t rm) { return I32(0x1ac02000 | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16)); }
static inline uint32_t lslv64(uint8_t rd, uint8_t rn, uint8_t rm) { return I32(0x9ac02000 | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16)); }
static inline uint32_t lsrv(uint8_t rd, uint8_t rn, uint8_t rm) { return I32(0x1ac02400 | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16)); }
static inline uint32_t lsrv64(uint8_t rd, uint8_t rn, uint8_t rm) { return I32(0x9ac02400 | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16)); }
static inline uint32_t rorv(uint8_t rd, uint8_t rn, uint8_t rm) { return I32(0x1ac02c00 | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16)); }
static inline uint32_t rorv64(uint8_t rd, uint8_t rn, uint8_t rm) { return I32(0x9ac02c00 | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16)); }

/* Data processing: multiply */
static inline uint32_t smaddl(uint8_t rd, uint8_t ra, uint8_t rn, uint8_t rm) { return I32(0x9b200000 | (rd & 31) | ((rn & 31) << 5) | ((ra & 31) << 10) | ((rm & 31) << 16)); }
static inline uint32_t smsubl(uint8_t rd, uint8_t ra, uint8_t rn, uint8_t rm) { return I32(0x9b208000 | (rd & 31) | ((rn & 31) << 5) | ((ra & 31) << 10) | ((rm & 31) << 16)); }
static inline uint32_t smnegl(uint8_t rd, uint8_t rn, uint8_t rm) { return smsubl(rd, 31, rn, rm); }
static inline uint32_t smull(uint8_t rd, uint8_t rn, uint8_t rm) { return smaddl(rd, 31, rn, rm); }
static inline uint32_t umaddl(uint8_t rd, uint8_t ra, uint8_t rn, uint8_t rm) { return I32(0x9ba00000 | (rd & 31) | ((rn & 31) << 5) | ((ra & 31) << 10) | ((rm & 31) << 16)); }
static inline uint32_t umsubl(uint8_t rd, uint8_t ra, uint8_t rn, uint8_t rm) { return I32(0x9ba08000 | (rd & 31) | ((rn & 31) << 5) | ((ra & 31) << 10) | ((rm & 31) << 16)); }
static inline uint32_t umnegl(uint8_t rd, uint8_t rn, uint8_t rm) { return umsubl(rd, 31, rn, rm); }
static inline uint32_t umull(uint8_t rd, uint8_t rn, uint8_t rm) { return umaddl(rd, 31, rn, rm); }
#endif

template< typename width=W32 >
static inline uint32_t MADD(Register<AArch64, INT> rd, Register<AArch64, INT> ra, Register<AArch64, INT> rn, Register<AArch64, INT> rm) { return I32(width::__width_value | 0x1b000000 | (rd.value() & 31) | ((rn.value() & 31) << 5) | ((ra.value() & 31) << 10) | ((rm.value() & 31) << 16)); }
template< typename width=W32 >
static inline uint32_t MSUB(Register<AArch64, INT> rd, Register<AArch64, INT> ra, Register<AArch64, INT> rn, Register<AArch64, INT> rm) { return I32(width::__width_value | 0x1b008000 | (rd.value() & 31) | ((rn.value() & 31) << 5) | ((ra.value() & 31) << 10) | ((rm.value() & 31) << 16)); }
template< typename width=W32 >
static inline uint32_t MNEG(Register<AArch64, INT> rd, Register<AArch64, INT> rn, Register<AArch64, INT> rm) { Register<AArch64, INT> zr(31, false); return MSUB<width>(rd, zr, rn, rm); }
template< typename width=W32 >
static inline uint32_t MUL(Register<AArch64, INT> rd, Register<AArch64, INT> rn, Register<AArch64, INT> rm) { Register<AArch64, INT> zr(31, false); return MADD<width>(rd, zr, rn, rm); }


/* Data processing: divide */
template< typename width=W32 >
static inline uint32_t SDIV(Register<AArch64, INT> rd, Register<AArch64, INT> rn, Register<AArch64, INT> rm) { return I32(width::__width_value | 0x1ac00c00 | (rd.value() & 31) | ((rn.value() & 31) << 5) | ((rm.value() & 31) << 16)); }
template< typename width=W32 >
static inline uint32_t UDIV(Register<AArch64, INT> rd, Register<AArch64, INT> rn, Register<AArch64, INT> rm) { return I32(width::__width_value | 0x1ac00800 | (rd.value() & 31) | ((rn.value() & 31) << 5) | ((rm.value() & 31) << 16)); }

/* Data processing: bit operations */
template< typename width=W32 >
static inline uint32_t CLS(Register<AArch64, INT> rd, Register<AArch64, INT> rn) { return I32(width::__width_value | 0x5ac01400 | (rd.value() & 31) | ((rn.value() & 31) << 5)); }
template< typename width=W32 >
static inline uint32_t CLZ(Register<AArch64, INT> rd, Register<AArch64, INT> rn) { return I32(width::__width_value | 0x5ac01000 | (rd.value() & 31) | ((rn.value() & 31) << 5)); }
template< typename width=W32 >
static inline uint32_t RBIT(Register<AArch64, INT> rd, Register<AArch64, INT> rn) { return I32(width::__width_value | 0x5ac00000 | (rd.value() & 31) | ((rn.value() & 31) << 5)); }
template< typename width=W32 >
static inline uint32_t REV(Register<AArch64, INT> rd, Register<AArch64, INT> rn) { return I32((width::__width_value ? 0xdac00c0 : 0x5ac00800) | (rd.value() & 31) | ((rn.value() & 31) << 5)); }
template< typename width=W32 >
static inline uint32_t REV16(Register<AArch64, INT> rd, Register<AArch64, INT> rn) { return I32(width::__width_value | 0x5ac00400 | (rd.value() & 31) | ((rn.value() & 31) << 5)); }
static inline uint32_t REV32(Register<AArch64, INT> rd, Register<AArch64, INT> rn) { return I32(0xdac00800 | (rd.value() & 31) | ((rn.value() & 31) << 5)); }

/* FPU/SIMD */
static inline uint32_t ABS(Register<AArch64, DOUBLE> rd, Register<AArch64, DOUBLE> rn) { return I32(0x1e60c000 | (rd.value() & 31) | ((rn.value() & 31) << 5)); }
static inline uint32_t ABS(Register<AArch64, SINGLE> rd, Register<AArch64, SINGLE> rn) { return I32(0x1e20c000 | (rd.value() & 31) | ((rn.value() & 31) << 5)); }
static inline uint32_t ADD(Register<AArch64, DOUBLE> rd, Register<AArch64, DOUBLE> first, Register<AArch64, DOUBLE> second) { return I32(0x1e602800 | (rd.value() & 31) | ((first.value() & 31) << 5) | ((second.value() & 31) << 16)); }
static inline uint32_t ADD(Register<AArch64, SINGLE> rd, Register<AArch64, SINGLE> first, Register<AArch64, SINGLE> second) { return I32(0x1e202800 | (rd.value() & 31) | ((first.value() & 31) << 5) | ((second.value() & 31) << 16)); }
static inline uint32_t CMP(Register<AArch64, DOUBLE> rd, Register<AArch64, DOUBLE> rn) { return I32(0x1e602000 | ((rd.value() & 31) << 5) | ((rn.value() & 31) << 16)); }
static inline uint32_t CMP(Register<AArch64, SINGLE> rd, Register<AArch64, SINGLE> rn) { return I32(0x1e202000 | ((rd.value() & 31) << 5) | ((rn.value() & 31) << 16)); }
static inline uint32_t CMPZ(Register<AArch64, DOUBLE> rn) { return INSN_TO_LE(0x1e602008 | ((rn.value() & 31) << 5)); }
static inline uint32_t CMPZ(Register<AArch64, SINGLE> rn) { return INSN_TO_LE(0x1e202008 | ((rn.value() & 31) << 5)); }
template< typename width=W32 >
static inline uint32_t CVT(Register<AArch64, DOUBLE> rd, Register<AArch64, INT> rn) { return I32(width::__width_value | 0x1e620000 | (rd.value() & 31) | ((rn.value() & 31) << 5)); }
template< typename width=W32 >
static inline uint32_t CVT(Register<AArch64, SINGLE> rd, Register<AArch64, INT> rn) { return I32(width::__width_value | 0x1e220000 | (rd.value() & 31) | ((rn.value() & 31) << 5)); }
template< typename width=W32 >
static inline uint32_t CVT(Register<AArch64, INT> rd, Register<AArch64, DOUBLE> rn) { return I32(width::__width_value | 0x1e780000 | (rd.value() & 31) | ((rn.value() & 31) << 5)); }
template< typename width=W32 >
static inline uint32_t CVT(Register<AArch64, INT> rd, Register<AArch64, SINGLE> rn) { return I32(width::__width_value | 0x1e380000 | (rd.value() & 31) | ((rn.value() & 31) << 5)); }
static inline uint32_t DIV(Register<AArch64, DOUBLE> rd, Register<AArch64, DOUBLE> dividend, Register<AArch64, DOUBLE> divisor) { return I32(0x1e601800 | (rd.value() & 31) | ((dividend.value() & 31) << 5) | ((divisor.value() & 31) << 16)); }
static inline uint32_t FRINT64X(Register<AArch64, DOUBLE> rd, Register<AArch64, DOUBLE> rn) { return I32(0x1e67c000 | (rd.value() & 31) | ((rn.value() & 31) << 5)); }
static inline uint32_t FRINT64Z(Register<AArch64, DOUBLE> rd, Register<AArch64, DOUBLE> rn) { return I32(0x1e65c000 | (rd.value() & 31) | ((rn.value() & 31) << 5)); }

template< typename scale=void >
static inline uint32_t LDR(Register<AArch64, DOUBLE> rd, Register<AArch64, INT> base, int16_t offset) { 
    if (std::is_same<scale, OFFSET_SCALED>::value) { return I32(0xfd400000 | ((base.value() & 31) << 5) | (rd.value() & 31) | ((offset & 0xfff) << 10));
    } else { return I32(0xfc400000 | ((base.value() & 31) << 5) | (rd.value() & 31) | ((offset & 0x1ff) << 12)); } }
template< typename scale=void >
static inline uint32_t LDR(Register<AArch64, SINGLE> rd, Register<AArch64, INT> base, int16_t offset) { 
    if (std::is_same<scale, OFFSET_SCALED>::value) { return I32(0xbd400000 | ((base.value() & 31) << 5) | (rd.value() & 31) | ((offset & 0xfff) << 10));
    } else { return I32(0xbc400000 | ((base.value() & 31) << 5) | (rd.value() & 31) | ((offset & 0x1ff) << 12)); } }
static inline uint32_t LDR(Register<AArch64, DOUBLE> rd, int32_t offset) { return I32(0x5c000000 | (rd.value() & 31) | ((offset & 0x7ffff) << 5)); }
static inline uint32_t LDR(Register<AArch64, SINGLE> rd, int32_t offset) { return I32(0x1c000000 | (rd.value() & 31) | ((offset & 0x7ffff) << 5)); }

static inline uint32_t MOV(Register<AArch64, DOUBLE> rd, Register<AArch64, DOUBLE> rn) { return I32(0x1e604000 | (rd.value() & 31) | ((rn.value() & 31) << 5)); }
static inline uint32_t MOV(Register<AArch64, SINGLE> rd, Register<AArch64, SINGLE> rn) { return I32(0x1e204000 | (rd.value() & 31) | ((rn.value() & 31) << 5)); }
static inline uint32_t MOV(Register<AArch64, DOUBLE> rd, Register<AArch64, SINGLE> rn) { return I32(0x1e22c000 | (rd.value() & 31) | ((rn.value() & 31) << 5)); }
static inline uint32_t MOV(Register<AArch64, SINGLE> rd, Register<AArch64, DOUBLE> rn) { return I32(0x1e624000 | (rd.value() & 31) | ((rn.value() & 31) << 5)); }
enum class TS : uint8_t { B = 1, H = 2, S = 4, D = 8 };
template < TS ts=TS::D, int index=0 >
static inline uint32_t MOV(Register<AArch64, INT> rd, Register<AArch64, DOUBLE> rn) { return I32((ts == TS::D ? 0x4e003c00 : 0x0e003c00) | (ts == TS::B ? ((index & 0xf) << 17) : ts == TS::H ? ((index & 7) << 18) : ts == TS::S ? ((index & 3) << 19) : ts == TS::D ? ((index & 1) << 20) : 0) | (static_cast<uint8_t>(ts) << 16) | (rd.value() & 31) | ((rn.value() & 31) << 5)); }
template < TS ts=TS::S, int index=0 >
static inline uint32_t MOV(Register<AArch64, INT> rd, Register<AArch64, SINGLE> rn) { return I32((ts == TS::D ? 0x4e003c00 : 0x0e003c00) | (ts == TS::B ? ((index & 0xf) << 17) : ts == TS::H ? ((index & 7) << 18) : ts == TS::S ? ((index & 3) << 19) : ts == TS::D ? ((index & 1) << 20) : 0) | (static_cast<uint8_t>(ts) << 16) | (rd.value() & 31) | ((rn.value() & 31) << 5)); }
template < TS ts=TS::D, int index=0 >
static inline uint32_t MOV(Register<AArch64, DOUBLE> rd, Register<AArch64, INT> rn) { return I32(0x4e001c00 | (ts == TS::B ? ((index & 0xf) << 17) : ts == TS::H ? ((index & 7) << 18) : ts == TS::S ? ((index & 3) << 19) : ts == TS::D ? ((index & 1) << 20) : 0) | (static_cast<uint8_t>(ts) << 16) | (rd.value() & 31) | ((rn.value() & 31) << 5)); }
template < TS ts=TS::S, int index=0 >
static inline uint32_t MOV(Register<AArch64, SINGLE> rd, Register<AArch64, INT> rn) { return I32(0x4e001c00 | (ts == TS::B ? ((index & 0xf) << 17) : ts == TS::H ? ((index & 7) << 18) : ts == TS::S ? ((index & 3) << 19) : ts == TS::D ? ((index & 1) << 20) : 0) | (static_cast<uint8_t>(ts) << 16) | (rd.value() & 31) | ((rn.value() & 31) << 5)); }
static inline uint32_t MOV(Register<AArch64, DOUBLE> rd, uint8_t imm) { return I32(0x1e601000 | (imm << 13) | (rd.value() & 31)); }
static inline uint32_t MOV(Register<AArch64, SINGLE> rd, uint8_t imm) { return I32(0x1e201000 | (imm << 13) | (rd.value() & 31)); }
static inline uint32_t MOVI(Register<AArch64, DOUBLE> rd, uint8_t imm) { return I32(0x2f00e400 | (rd.value() & 31) | ((imm & 31) << 5) | (((imm > 5) & 7) << 16)); }
static inline uint32_t MOV_0(Register<AArch64, DOUBLE> rd) { return MOVI(rd, 0); }
static inline uint32_t MOV_1(Register<AArch64, DOUBLE> rd) { return MOV(rd, 112); }
static inline uint32_t MUL(Register<AArch64, DOUBLE> rd, Register<AArch64, DOUBLE> first, Register<AArch64, DOUBLE> second) { return I32(0x1e600800 | (rd.value() & 31) | ((first.value() & 31) << 5) | ((second.value() & 31) << 16)); }
static inline uint32_t NEG(Register<AArch64, DOUBLE> rd, Register<AArch64, DOUBLE> rn) { return I32(0x1e614000 | (rd.value() & 31) | ((rn.value() & 31) << 5)); }
static inline uint32_t SQRT(Register<AArch64, DOUBLE> rd, Register<AArch64, DOUBLE> rn) { return I32(0x1e61c000 | (rd.value() & 31) | ((rn.value() & 31) << 5)); }
static inline uint32_t SUB(Register<AArch64, DOUBLE> rd, Register<AArch64, DOUBLE> first, Register<AArch64, DOUBLE> second) { return I32(0x1e603800 | (rd.value() & 31) | ((first.value() & 31) << 5) | ((second.value() & 31) << 16)); }
template< typename scale=void >
static inline uint32_t STR(Register<AArch64, DOUBLE> rd, Register<AArch64, INT> base, int16_t offset) { 
    if (std::is_same<scale, OFFSET_SCALED>::value) { return I32(0xfd000000 | ((base.value() & 31) << 5) | (rd.value() & 31) | ((offset & 0xfff) << 10));
    } else { return I32(0xfc000000 | ((base.value() & 31) << 5) | (rd.value() & 31) | ((offset & 0x1ff) << 12)); } }
template< typename scale=void >
static inline uint32_t STR(Register<AArch64, SINGLE> rd, Register<AArch64, INT> base, int16_t offset) { 
    if (std::is_same<scale, OFFSET_SCALED>::value) { return I32(0xbd000000 | ((base.value() & 31) << 5) | (rd.value() & 31) | ((offset & 0xfff) << 10));
    } else { return I32(0xbc000000 | ((base.value() & 31) << 5) | (rd.value() & 31) | ((offset & 0x1ff) << 12)); } }


}

#endif /* _EMU64_AARCH64_OPCODES_H */
