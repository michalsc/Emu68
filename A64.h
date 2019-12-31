/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _A64_H
#define _A64_H

#include <stdint.h>

#define REG_PC    16
#define REG_SR    17
#define REG_CTX   18

#define REG_D0    19
#define REG_D1    20
#define REG_D2    21
#define REG_D3    22
#define REG_D4    23
#define REG_D5    24
#define REG_D6    25
#define REG_D7    26

#define REG_A0    11
#define REG_A1    12
#define REG_A2    13
#define REG_A3    14
#define REG_A4    15
#define REG_A5    27
#define REG_A6    28
#define REG_A7    29

#define A64_CC_EQ 0x00 /* Z=1 */
#define A64_CC_NE 0x01 /* Z=0 */
#define A64_CC_CS 0x02 /* C=1 */
#define A64_CC_CC 0x03 /* C=0 */
#define A64_CC_MI 0x04 /* N=1 */
#define A64_CC_PL 0x05 /* N=0 */
#define A64_CC_VS 0x06 /* V=1 */
#define A64_CC_VC 0x07 /* V=0 */
#define A64_CC_HI 0x08
#define A64_CC_LS 0x09
#define A64_CC_GE 0x0a /* N == V */
#define A64_CC_LT 0x0b /* N != V */
#define A64_CC_GT 0x0c /* Z == 0 && N == V */
#define A64_CC_LE 0x0d /* Z == 1 || N != V */
#define A64_CC_AL 0x0e /* Always */
#define A64_CC_NV 0x0f /* Always */

#define SP      31  /* 31 encodes SP base, or */
#define ZR      31  /* Zero register, depending on usage */


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

/* Branches */
static inline uint32_t b_cc(uint8_t cc, int32_t offset19) { return I32(0x54000000 | (cc & 15) | ((offset19 & 0x7ffff) << 5)); }
static inline uint32_t b(uint32_t offset) { return I32(0x14000000 | (offset & 0x3ffffff)); }
static inline uint32_t bl(uint32_t offset) { return I32(0x94000000 | (offset & 0x3ffffff)); }
static inline uint32_t blr(uint8_t rt) { return I32(0xd63f0000 | ((rt & 31) << 5));}
static inline uint32_t br(uint8_t rt) { return I32(0xd61f0000 | ((rt & 31) << 5));}
static inline uint32_t cbnz(uint8_t rt, uint32_t offset19) { return I32(0x35000000 | ((offset19 & 0x7ffff) << 5) | (rt & 31)); }
static inline uint32_t cbnz_64(uint8_t rt, uint32_t offset19) { return I32(0xb5000000 | ((offset19 & 0x7ffff) << 5) | (rt & 31)); }
static inline uint32_t cbz(uint8_t rt, uint32_t offset19) { return I32(0x34000000 | ((offset19 & 0x7ffff) << 5) | (rt & 31)); }
static inline uint32_t cbz_64(uint8_t rt, uint32_t offset19) { return I32(0xb4000000 | ((offset19 & 0x7ffff) << 5) | (rt & 31)); }
static inline uint32_t ret() { return I32(0xd62f0000 | (30 << 5));}
static inline uint32_t ret_reg(uint8_t rt) { return I32(0xd62f0000 | ((rt & 31) << 5));}
static inline uint32_t tbnz(uint8_t rt, uint8_t bit, uint16_t offset) { return I32(bit & 32 ? 0xb7000000 : 0x37000000 | ((bit & 31) << 19) | ((offset & 0x3fff) << 5) | (rt & 31)); }
static inline uint32_t tbz(uint8_t rt, uint8_t bit, uint16_t offset) { return I32(bit & 32 ? 0xb6000000 : 0x36000000 | ((bit & 31) << 19) | ((offset & 0x3fff) << 5) | (rt & 31)); }

/* Load/Store instructions */
static inline uint32_t ldr_pcrel(uint8_t rt, int32_t offset19) { return I32(0x18000000 | ((offset19 & 0x7ffff) << 5) | (rt & 31)); }
static inline uint32_t ldr64_pcrel(uint8_t rt, int32_t offset19) { return I32(0x58000000 | ((offset19 & 0x7ffff) << 5) | (rt & 31)); }


#endif /* _A64_H */
