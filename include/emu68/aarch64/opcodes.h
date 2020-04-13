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


}

#endif /* _EMU64_AARCH64_OPCODES_H */
