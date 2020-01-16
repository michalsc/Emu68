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

/* Context pointer is stored in TPIDRRO_EL0 */

#define REG_PC    18

#define REG_D0    19
#define REG_D1    20
#define REG_D2    21
#define REG_D3    22
#define REG_D4    23
#define REG_D5    24
#define REG_D6    25
#define REG_D7    26

#define REG_A0    13
#define REG_A1    14
#define REG_A2    15
#define REG_A3    16
#define REG_A4    17
#define REG_A5    27
#define REG_A6    28
#define REG_A7    29

#define REG_FP0   8
#define REG_FP1   9
#define REG_FP2   10
#define REG_FP3   11
#define REG_FP4   12
#define REG_FP5   13
#define REG_FP6   14
#define REG_FP7   15

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

#define MMU_NG          0x800
#define MMU_ACCESS      0x400
#define MMU_ISHARE      0x300
#define MMU_OSHARE      0x200
#define MMU_READ_ONLY   0x080
#define MMU_ALLOW_EL0   0x040
#define MMU_NS          0x020
#define MMU_ATTR(x)     (((x) & 7) << 2)
#define MMU_DIR         0x003
#define MMU_PAGE        0x001

#define ATTR_DEVICE_nGnRnE  0x00
#define ATTR_DEVICE_nGnRE   0x04
#define ATTR_NOCACHE        0x44
#define ATTR_CACHED         0xff

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

/* System instructions */
static inline uint32_t mrs(uint8_t rt, uint8_t op0, uint8_t op1, uint8_t crn, uint8_t crm, uint8_t op2) { return I32(0xd5300000 | (rt & 31) | (op0 == 3 ? 0x80000 : 0) | ((op1 & 7) << 16) | ((crn & 15) << 12) | ((crm & 15) << 8) | ((op2 & 7) << 5)); }
static inline uint32_t msr(uint8_t rt, uint8_t op0, uint8_t op1, uint8_t crn, uint8_t crm, uint8_t op2) { return I32(0xd5100000 | (rt & 31) | (op0 == 3 ? 0x80000 : 0) | ((op1 & 7) << 16) | ((crn & 15) << 12) | ((crm & 15) << 8) | ((op2 & 7) << 5)); }

/* Load/Store instructions */
static inline uint32_t ldr_pcrel(uint8_t rt, int32_t offset19) { return I32(0x18000000 | ((offset19 & 0x7ffff) << 5) | (rt & 31)); }
static inline uint32_t ldr64_pcrel(uint8_t rt, int32_t offset19) { return I32(0x58000000 | ((offset19 & 0x7ffff) << 5) | (rt & 31)); }
static inline uint32_t ldrsw_pcrel(uint8_t rt, int32_t offset19) { return I32(0x98000000 | ((offset19 & 0x7ffff) << 5) | (rt & 31)); }

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

/* Data processing */
static inline uint32_t mov_immed_u16(uint8_t reg, uint16_t val, uint8_t shift16) { return I32(0x52800000 | ((shift16 & 3) << 21) | (val << 5) | (reg & 31)); }
static inline uint32_t mov64_immed_u16(uint8_t reg, uint16_t val, uint8_t shift16) { return I32(0xd2800000 | ((shift16 & 3) << 21) | (val << 5) | (reg & 31)); }
static inline uint32_t movk_immed_u16(uint8_t reg, uint16_t val, uint8_t shift16) { return I32(0x72800000 | ((shift16 & 3) << 21) | (val << 5) | (reg & 31)); }
static inline uint32_t movk64_immed_u16(uint8_t reg, uint16_t val, uint8_t shift16) { return I32(0xf2800000 | ((shift16 & 3) << 21) | (val << 5) | (reg & 31)); }
static inline uint32_t movn_immed_u16(uint8_t reg, uint16_t val, uint8_t shift16) { return I32(0x12800000 | ((shift16 & 3) << 21) | (val << 5) | (reg & 31)); }
static inline uint32_t movn64_immed_u16(uint8_t reg, uint16_t val, uint8_t shift16) { return I32(0x92800000 | ((shift16 & 3) << 21) | (val << 5) | (reg & 31)); }
static inline uint32_t movw_immed_u16(uint8_t reg, uint16_t val) { return mov_immed_u16(reg, val, 0); }
static inline uint32_t movt_immed_u16(uint8_t reg, uint16_t val) { return movk_immed_u16(reg, val, 1); }



#endif /* _A64_H */
