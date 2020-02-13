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

#define ARM_CC_EQ 0x00 /* Z=1 */
#define ARM_CC_NE 0x01 /* Z=0 */
#define ARM_CC_CS 0x02 /* C=1 */
#define ARM_CC_CC 0x03 /* C=0 */
#define ARM_CC_MI 0x04 /* N=1 */
#define ARM_CC_PL 0x05 /* N=0 */
#define ARM_CC_VS 0x06 /* V=1 */
#define ARM_CC_VC 0x07 /* V=0 */
#define ARM_CC_HI 0x08
#define ARM_CC_LS 0x09
#define ARM_CC_GE 0x0a /* N == V */
#define ARM_CC_LT 0x0b /* N != V */
#define ARM_CC_GT 0x0c /* Z == 0 && N == V */
#define ARM_CC_LE 0x0d /* Z == 1 || N != V */
#define ARM_CC_AL 0x0e /* Always */
#define ARM_CC_NV 0x0f /* Always */

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

static inline uint32_t INSN_TO_LE(uint32_t insn)
{
    return I32(insn);
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
static inline uint32_t bx_lr() { return br(30); }

/* System instructions */
static inline uint32_t mrs(uint8_t rt, uint8_t op0, uint8_t op1, uint8_t crn, uint8_t crm, uint8_t op2) { return I32(0xd5300000 | (rt & 31) | (op0 == 3 ? 0x80000 : 0) | ((op1 & 7) << 16) | ((crn & 15) << 12) | ((crm & 15) << 8) | ((op2 & 7) << 5)); }
static inline uint32_t msr(uint8_t rt, uint8_t op0, uint8_t op1, uint8_t crn, uint8_t crm, uint8_t op2) { return I32(0xd5100000 | (rt & 31) | (op0 == 3 ? 0x80000 : 0) | ((op1 & 7) << 16) | ((crn & 15) << 12) | ((crm & 15) << 8) | ((op2 & 7) << 5)); }
static inline uint32_t brk(uint16_t imm16) { return I32(0xd4200000 | (imm16 << 5)); }
static inline uint32_t hlt(uint16_t imm16) { return I32(0xd4400000 | (imm16 << 5)); }
static inline uint32_t udf(uint16_t imm16) { return hlt(imm16); }
static inline uint32_t hint(uint8_t h) { return I32(0xd503201f | ((h & 0x7f) << 5)); }
static inline uint32_t get_nzcv(uint8_t rt) { return mrs(rt, 3, 3, 4, 2, 0); }
static inline uint32_t set_nzcv(uint8_t rt) { return msr(rt, 3, 3, 4, 2, 0); }

/* Load PC-relatve address */
static inline uint32_t adr(uint8_t rd, uint32_t imm21) { return I32(0x10000000 | (rd & 31) | ((imm21 & 3) << 29) | (((imm21 >> 2) & 0x7ffff) << 5)); }
static inline uint32_t adrp(uint8_t rd, uint32_t imm21) { return I32(0x90000000 | (rd & 31) | ((imm21 & 3) << 29) | (((imm21 >> 2) & 0x7ffff) << 5)); }

/* Load/Store instructions */
static inline uint32_t ldr_pcrel(uint8_t rt, int32_t offset19) { return I32(0x18000000 | ((offset19 & 0x7ffff) << 5) | (rt & 31)); }
static inline uint32_t ldr64_pcrel(uint8_t rt, int32_t offset19) { return I32(0x58000000 | ((offset19 & 0x7ffff) << 5) | (rt & 31)); }
static inline uint32_t ldrsw_pcrel(uint8_t rt, int32_t offset19) { return I32(0x98000000 | ((offset19 & 0x7ffff) << 5) | (rt & 31)); }

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
static inline uint32_t stp(uint8_t rn, uint8_t rt1, uint8_t rt2, int8_t imm) { return I32(0x29000000 | (rt1 & 31) | ((rt2 & 31) << 10) | ((rn & 31) << 5) | (((imm / 4) & 0x7f) << 15)); }
static inline uint32_t stp64(uint8_t rn, uint8_t rt1, uint8_t rt2, int8_t imm) { return I32(0xa9000000 | (rt1 & 31) | ((rt2 & 31) << 10) | ((rn & 31) << 5) | (((imm / 8) & 0x7f) << 15)); }

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
static inline uint32_t madd(uint8_t rd, uint8_t ra, uint8_t rn, uint8_t rm) { return I32(0x1b000000 | (rd & 31) | ((rn & 31) << 5) | ((ra & 31) << 10) | ((rm & 31) << 16)); }
static inline uint32_t madd64(uint8_t rd, uint8_t ra, uint8_t rn, uint8_t rm) { return I32(0x9b000000 | (rd & 31) | ((rn & 31) << 5) | ((ra & 31) << 10) | ((rm & 31) << 16)); }
static inline uint32_t msub(uint8_t rd, uint8_t ra, uint8_t rn, uint8_t rm) { return I32(0x1b008000 | (rd & 31) | ((rn & 31) << 5) | ((ra & 31) << 10) | ((rm & 31) << 16)); }
static inline uint32_t msub64(uint8_t rd, uint8_t ra, uint8_t rn, uint8_t rm) { return I32(0x9b008000 | (rd & 31) | ((rn & 31) << 5) | ((ra & 31) << 10) | ((rm & 31) << 16)); }
static inline uint32_t mneg(uint8_t rd, uint8_t rn, uint8_t rm) { return msub(rd, 31, rn, rm); }
static inline uint32_t mneg64(uint8_t rd, uint8_t rn, uint8_t rm) { return msub64(rd, 31, rn, rm); }
static inline uint32_t mul(uint8_t rd, uint8_t rn, uint8_t rm) { return madd(rd, 31, rn, rm); }
static inline uint32_t mul64(uint8_t rd, uint8_t rn, uint8_t rm) { return madd64(rd, 31, rn, rm); }
static inline uint32_t smaddl(uint8_t rd, uint8_t ra, uint8_t rn, uint8_t rm) { return I32(0x9b200000 | (rd & 31) | ((rn & 31) << 5) | ((ra & 31) << 10) | ((rm & 31) << 16)); }
static inline uint32_t smsubl(uint8_t rd, uint8_t ra, uint8_t rn, uint8_t rm) { return I32(0x9b208000 | (rd & 31) | ((rn & 31) << 5) | ((ra & 31) << 10) | ((rm & 31) << 16)); }
static inline uint32_t smnegl(uint8_t rd, uint8_t rn, uint8_t rm) { return smsubl(rd, 31, rn, rm); }
static inline uint32_t smull(uint8_t rd, uint8_t rn, uint8_t rm) { return smaddl(rd, 31, rn, rm); }
static inline uint32_t umaddl(uint8_t rd, uint8_t ra, uint8_t rn, uint8_t rm) { return I32(0x9ba00000 | (rd & 31) | ((rn & 31) << 5) | ((ra & 31) << 10) | ((rm & 31) << 16)); }
static inline uint32_t umsubl(uint8_t rd, uint8_t ra, uint8_t rn, uint8_t rm) { return I32(0x9ba08000 | (rd & 31) | ((rn & 31) << 5) | ((ra & 31) << 10) | ((rm & 31) << 16)); }
static inline uint32_t umnegl(uint8_t rd, uint8_t rn, uint8_t rm) { return umsubl(rd, 31, rn, rm); }
static inline uint32_t umull(uint8_t rd, uint8_t rn, uint8_t rm) { return umaddl(rd, 31, rn, rm); }

/* Data processing: divide */
static inline uint32_t sdiv(uint8_t rd, uint8_t rn, uint8_t rm) { return I32(0x1ac00c00 | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16)); }
static inline uint32_t sdiv64(uint8_t rd, uint8_t rn, uint8_t rm) { return I32(0x9ac00c00 | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16)); }
static inline uint32_t udiv(uint8_t rd, uint8_t rn, uint8_t rm) { return I32(0x1ac00800 | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16)); }
static inline uint32_t udiv64(uint8_t rd, uint8_t rn, uint8_t rm) { return I32(0x9ac00800 | (rd & 31) | ((rn & 31) << 5) | ((rm & 31) << 16)); }

/* Data processing: bit operations */
static inline uint32_t cls(uint8_t rd, uint8_t rn) { return I32(0x5ac01400 | (rd & 31) | ((rn & 31) << 5)); }
static inline uint32_t cls64(uint8_t rd, uint8_t rn) { return I32(0xdac01400 | (rd & 31) | ((rn & 31) << 5)); }
static inline uint32_t clz(uint8_t rd, uint8_t rn) { return I32(0x5ac01000 | (rd & 31) | ((rn & 31) << 5)); }
static inline uint32_t clz64(uint8_t rd, uint8_t rn) { return I32(0xdac01000 | (rd & 31) | ((rn & 31) << 5)); }
static inline uint32_t rbit(uint8_t rd, uint8_t rn) { return I32(0x5ac00000 | (rd & 31) | ((rn & 31) << 5)); }
static inline uint32_t rbit64(uint8_t rd, uint8_t rn) { return I32(0xdac00000 | (rd & 31) | ((rn & 31) << 5)); }
static inline uint32_t rev(uint8_t rd, uint8_t rn) { return I32(0x5ac00800 | (rd & 31) | ((rn & 31) << 5)); }
static inline uint32_t rev16(uint8_t rd, uint8_t rn) { return I32(0x5ac00400 | (rd & 31) | ((rn & 31) << 5)); }
static inline uint32_t rev16_64(uint8_t rd, uint8_t rn) { return I32(0xdac00400 | (rd & 31) | ((rn & 31) << 5)); }
static inline uint32_t rev32(uint8_t rd, uint8_t rn) { return I32(0xdac00800 | (rd & 31) | ((rn & 31) << 5)); }
static inline uint32_t rev64(uint8_t rd, uint8_t rn) { return I32(0xdac00c00 | (rd & 31) | ((rn & 31) << 5)); }

/* Floating point */
static inline uint32_t fabsd(uint8_t v_dst, uint8_t v_src) { return I32(0x1e60c000 | (v_dst & 31) | ((v_src & 31) << 5)); }
static inline uint32_t fabss(uint8_t v_dst, uint8_t v_src) { return I32(0x1e20c000 | (v_dst & 31) | ((v_src & 31) << 5)); }
static inline uint32_t faddd(uint8_t v_dst, uint8_t v_first, uint8_t v_second) { return I32(0x1e602800 | (v_dst & 31) | ((v_first & 31) << 5) | ((v_second & 31) << 16)); }
static inline uint32_t fadds(uint8_t v_dst, uint8_t v_first, uint8_t v_second) { return I32(0x1e202800 | (v_dst & 31) | ((v_first & 31) << 5) | ((v_second & 31) << 16)); }
static inline uint32_t fcmpd(uint8_t v_dst, uint8_t v_src) { return INSN_TO_LE(0x1e602000 | ((v_dst & 31) << 5) | ((v_src & 31) << 16)); }
static inline uint32_t fcmps(uint8_t v_dst, uint8_t v_src) { return INSN_TO_LE(0x1e202000 | ((v_dst & 31) << 5) | ((v_src & 31) << 16)); }
static inline uint32_t fcmpzd(uint8_t v_src) { return INSN_TO_LE(0x1e602008 | ((v_src & 31) << 5)); }
static inline uint32_t fcmpzs(uint8_t v_src) { return INSN_TO_LE(0x1e202008 | ((v_src & 31) << 5)); }
static inline uint32_t fcpyd(uint8_t v_dst, uint8_t v_src) { return I32(0x1e604000 | (v_dst & 31) | ((v_src & 31) << 5)); }
static inline uint32_t fcvtds(uint8_t d_dst, uint8_t s_src) { return I32(0x1e22c000 | (d_dst & 31) | ((s_src & 31) << 5)); }
static inline uint32_t fcvtsd(uint8_t s_dst, uint8_t d_src) { return I32(0x1e624000 | (s_dst & 31) | ((d_src & 31) << 5)); }
static inline uint32_t fdivd(uint8_t v_dst, uint8_t v_dividend, uint8_t v_divisor) { return I32(0x1e601800 | (v_dst & 31) | ((v_dividend & 31) << 5) | ((v_divisor & 31) << 16)); }
static inline uint32_t fldd(uint8_t v_dst, uint8_t base, int16_t offset9) { return I32(0xfc400000 | ((base & 31) << 5) | (v_dst & 31) | ((offset9 & 0x1ff) < 12)); }
static inline uint32_t flds(uint8_t v_dst, uint8_t base, int16_t offset9) { return I32(0xbc400000 | ((base & 31) << 5) | (v_dst & 31) | ((offset9 & 0x1ff) < 12)); }

enum TS { TS_B = 1, TS_H = 2, TS_S = 4, TS_D = 8 };
static inline uint32_t mov_reg_to_simd(uint8_t v_dst, enum TS ts, uint8_t index, uint8_t rn) { return I32(0x4e001c00 | (ts == TS_B ? ((index & 0xf) << 17) : ts == TS_H ? ((index & 7) << 18) : ts == TS_S ? ((index & 3) << 19) : ts == TS_D ? ((index & 1) << 20) : 0) | ((ts & 31) << 16) | (v_dst & 31) | ((rn & 31) << 5)); }
static inline uint32_t mov_simd_to_reg(uint8_t rd, uint8_t v_src, enum TS ts, uint8_t index) { return I32(ts == TS_D ? 0x4e003c00 : 0x0e003c00 | (ts == TS_B ? ((index & 0xf) << 17) : ts == TS_H ? ((index & 7) << 18) : ts == TS_S ? ((index & 3) << 19) : ts == TS_D ? ((index & 1) << 20) : 0) | ((ts & 31) << 16) | (rd & 31) | ((v_src & 31) << 5)); }
static inline uint32_t fmsr(uint8_t v_dst, uint8_t src) { return mov_reg_to_simd(v_dst, TS_S, 0, src); }

static inline uint32_t fmuld(uint8_t v_dst, uint8_t v_first, uint8_t v_second) { return I32(0x1e600800 | (v_dst & 31) | ((v_first & 31) << 5) | ((v_second & 31) << 16)); }
static inline uint32_t fnegd(uint8_t v_dst, uint8_t v_src) { return I32(0x1e614000 | (v_dst & 31) | ((v_src & 31) << 5)); }
static inline uint32_t fsqrtd(uint8_t v_dst, uint8_t v_src) { return I32(0x1e61c000 | (v_dst & 31) | ((v_src & 31) << 5)); }
static inline uint32_t fsubd(uint8_t v_dst, uint8_t v_first, uint8_t v_second) { return I32(0x1e603800 | (v_dst & 31) | ((v_first & 31) << 5) | ((v_second & 31) << 16)); }
static inline uint32_t fstd(uint8_t v_dst, uint8_t base, int16_t offset9) { return I32(0xfc000000 | ((base & 31) << 5) | (v_dst & 31) | ((offset9 & 0x1ff) < 12)); }
static inline uint32_t fsts(uint8_t v_dst, uint8_t base, int16_t offset9) { return I32(0xbc000000 | ((base & 31) << 5) | (v_dst & 31) | ((offset9 & 0x1ff) < 12)); }

#if 0
static inline uint32_t fmsr_cc(uint8_t cc, uint8_t v_dst, uint8_t src) { return INSN_TO_LE(0x0e000a10 | (cc << 28) | ((v_dst >> 1) << 16) | (src << 12) | ((v_dst & 1) << 7)); }
static inline uint32_t fmsr(uint8_t v_dst, uint8_t src) { return fmsr_cc(ARM_CC_AL, v_dst, src); }
static inline uint32_t fmdhr_cc(uint8_t cc, uint8_t v_dst, uint8_t src) { return INSN_TO_LE(0x0e200b10 | (cc << 28) | (v_dst << 16) | (src << 12)); }
static inline uint32_t fmdhr(uint8_t v_dst, uint8_t src) { return fmdhr_cc(ARM_CC_AL, v_dst, src); }
static inline uint32_t fmdlr_cc(uint8_t cc, uint8_t v_dst, uint8_t src) { return INSN_TO_LE(0x0e000b10 | (cc << 28) | (v_dst << 16) | (src << 12)); }
static inline uint32_t fmdlr(uint8_t v_dst, uint8_t src) { return fmdlr_cc(ARM_CC_AL, v_dst, src); }
static inline uint32_t fmdrr_cc(uint8_t cc, uint8_t v_dst, uint8_t src_hi, uint8_t src_lo) { return INSN_TO_LE(0x0c400b10 | (cc << 28) | (v_dst) | (src_lo << 12) | (src_hi << 16)); }
static inline uint32_t fmdrr(uint8_t v_dst, uint8_t src_hi, uint8_t src_lo) { return fmdrr_cc(ARM_CC_AL, v_dst, src_hi, src_lo); }
enum VFP_REG { FPSID = 0, FPSCR = 1, FPEXT = 8 };
static inline uint32_t fmrx_cc(uint8_t cc, enum VFP_REG sr, uint8_t src) { return INSN_TO_LE(0x0ee00a10 | (cc << 28) | (src << 12) | (sr << 16)); }
static inline uint32_t fmrx(enum VFP_REG sr, uint8_t src) { return fmrx_cc(ARM_CC_AL, sr, src); }
static inline uint32_t fmxr_cc(uint8_t cc, uint8_t dest, enum VFP_REG sr) { return INSN_TO_LE(0x0ef00a10 | (cc << 28) | (dest << 12) | (sr << 16)); }
static inline uint32_t fmxr(uint8_t dest, enum VFP_REG sr) { return fmxr_cc(ARM_CC_AL, dest, sr); }
static inline uint32_t fmov_cc_imm(uint8_t cc, uint8_t dst, uint8_t imm) { return INSN_TO_LE(0x0eb00b00 | (cc << 28) | ((imm >> 4) << 16) | (dst << 12) | (imm & 0xf)); }
static inline uint32_t fmov_imm(uint8_t dst, uint8_t imm) { return fmov_cc_imm(ARM_CC_AL, dst, imm); }
static inline uint32_t fmov_i64(uint8_t dst, uint8_t imm, uint8_t cmode) { return INSN_TO_LE(0xf2800e30 | ((imm >> 7) << 24) | (((imm >> 4) & 7) << 16) | (dst << 12) | (imm & 0xf) | (cmode << 8)); }
static inline uint32_t fmrrd_cc(uint8_t cc, uint8_t dst_hi, uint8_t dst_lo, uint8_t v_src) { return INSN_TO_LE(0x0c500b10 | (cc << 28) | (v_src) | (dst_lo << 12) | (dst_hi << 16)); }
static inline uint32_t fmrrd(uint8_t dst_hi, uint8_t dst_lo, uint8_t v_src) { return fmrrd_cc(ARM_CC_AL, dst_hi, dst_lo, v_src); }
static inline uint32_t fmrs_cc(uint8_t cc, uint8_t dst, uint8_t v_src) { return INSN_TO_LE(0x0e100a10 | (cc << 28) | ((v_src >> 1) << 16) | (dst << 12) | ((v_src & 1) << 7)); }
static inline uint32_t fmrs(uint8_t dst, uint8_t v_src) { return fmrs_cc(ARM_CC_AL, dst, v_src); }
static inline uint32_t fmstat_cc(uint8_t cc) { return INSN_TO_LE(0x0ef1fa10 | (cc << 28)); }
static inline uint32_t fmstat() { return fmstat_cc(ARM_CC_AL); }
static inline uint32_t fsitod_cc(uint8_t cc, uint8_t d_dst, uint8_t s_src) { return INSN_TO_LE(0x0eb80bc0 | (cc << 28) | (d_dst << 12) | (s_src >> 1) | (s_src & 1 ? 0x20:0)); }
static inline uint32_t fsitod(uint8_t d_dst, uint8_t s_src) { return fsitod_cc(ARM_CC_AL, d_dst, s_src); }
static inline uint32_t ftosid_cc(uint8_t cc, uint8_t s_dst, uint8_t d_src) { return INSN_TO_LE(0x0ebd0b40 | (cc << 28) | ((s_dst >> 1) << 12) | d_src | (s_dst & 1 ? (1 << 22):0)); }
static inline uint32_t ftosid(uint8_t s_dst, uint8_t d_src) { return ftosid_cc(ARM_CC_AL, s_dst, d_src); }
static inline uint32_t ftosidrz_cc(uint8_t cc, uint8_t s_dst, uint8_t d_src) { return INSN_TO_LE(0x0ebd0bc0 | (cc << 28) | ((s_dst >> 1) << 12) | d_src | (s_dst & 1 ? (1 << 22):0)); }
static inline uint32_t ftosidrz(uint8_t s_dst, uint8_t d_src) { return ftosidrz_cc(ARM_CC_AL, s_dst, d_src); }
#endif


#include <RegisterAllocator.h>

static inline __attribute__((always_inline))
uint32_t * EMIT_GetNZVC(uint32_t * ptr, uint8_t cc, uint8_t *not_done)
{
    (void)cc;
    *not_done = 15;
    return ptr;
}

static inline __attribute__((always_inline))
uint32_t * EMIT_GetNZ00(uint32_t * ptr, uint8_t cc, uint8_t *not_done)
{
    uint8_t tmp_reg = RA_AllocARMRegister(&ptr);

    *ptr++ = get_nzcv(tmp_reg);
    *ptr++ = bfxil(cc, tmp_reg, 28, 4);
    *ptr++ = bic_immed(cc, cc, 2, 0);

    RA_FreeARMRegister(&ptr, tmp_reg);

    *not_done = 0;

    return ptr;
}

static inline __attribute__((always_inline))
uint32_t * EMIT_GetNZxx(uint32_t * ptr, uint8_t cc, uint8_t *not_done)
{
    uint8_t tmp_reg = RA_AllocARMRegister(&ptr);

    *ptr++ = get_nzcv(tmp_reg);
    *ptr++ = ror(tmp_reg, tmp_reg, 30);
    *ptr++ = bfi(cc, tmp_reg, 2, 2);

    RA_FreeARMRegister(&ptr, tmp_reg);

    *not_done = 0;
    return ptr;
}

static inline __attribute__((always_inline))
uint32_t * EMIT_ClearFlags(uint32_t * ptr, uint8_t cc, uint8_t flags)
{
    uint8_t tmp_reg;

    switch (flags)
    {
        case 0:
            break;

        case 1:
            *ptr++ = bic_immed(cc, cc, 1, 0);
            break;

        case 3:
            *ptr++ = bic_immed(cc, cc, 2, 0);
            break;

        case 7:
            *ptr++ = bic_immed(cc, cc, 3, 0);
            break;

        case 15:
            *ptr++ = bic_immed(cc, cc, 4, 0);
            break;

        case 31:
            *ptr++ = bic_immed(cc, cc, 5, 0);
            break;

        case 2:
            *ptr++ = bic_immed(cc, cc, 1, 31);
            break;

        case 6:
            *ptr++ = bic_immed(cc, cc, 2, 31);
            break;

        case 14:
            *ptr++ = bic_immed(cc, cc, 3, 31);
            break;

        case 30:
            *ptr++ = bic_immed(cc, cc, 4, 31);
            break;

        case 4:
            *ptr++ = bic_immed(cc, cc, 1, 30);
            break;

        case 12:
            *ptr++ = bic_immed(cc, cc, 2, 30);
            break;

        case 28:
            *ptr++ = bic_immed(cc, cc, 3, 30);
            break;

        case 8:
            *ptr++ = bic_immed(cc, cc, 1, 29);
            break;

        case 24:
            *ptr++ = bic_immed(cc, cc, 2, 29);
            break;

        case 16:
            *ptr++ = bic_immed(cc, cc, 1, 28);
            break;

        default:
            tmp_reg = RA_AllocARMRegister(&ptr);
            *ptr++ = mov_immed_u16(tmp_reg, flags, 0);
            *ptr++ = bic_reg(cc, cc, tmp_reg, LSL, 0);
            RA_FreeARMRegister(&ptr, tmp_reg);
    }

    return ptr;
}

static inline __attribute__((always_inline))
uint32_t * EMIT_SetFlags(uint32_t * ptr, uint8_t cc, uint8_t flags)
{
    uint8_t tmp_reg;

    switch (flags)
    {
        case 0:
            break;

        case 1:
            *ptr++ = orr_immed(cc, cc, 1, 0);
            break;

        case 3:
            *ptr++ = orr_immed(cc, cc, 2, 0);
            break;

        case 7:
            *ptr++ = orr_immed(cc, cc, 3, 0);
            break;

        case 15:
            *ptr++ = orr_immed(cc, cc, 4, 0);
            break;

        case 31:
            *ptr++ = orr_immed(cc, cc, 5, 0);
            break;

        case 2:
            *ptr++ = orr_immed(cc, cc, 1, 31);
            break;

        case 6:
            *ptr++ = orr_immed(cc, cc, 2, 31);
            break;

        case 14:
            *ptr++ = orr_immed(cc, cc, 3, 31);
            break;

        case 30:
            *ptr++ = orr_immed(cc, cc, 4, 31);
            break;

        case 4:
            *ptr++ = orr_immed(cc, cc, 1, 30);
            break;

        case 12:
            *ptr++ = orr_immed(cc, cc, 2, 30);
            break;

        case 28:
            *ptr++ = orr_immed(cc, cc, 3, 30);
            break;

        case 8:
            *ptr++ = orr_immed(cc, cc, 1, 29);
            break;

        case 24:
            *ptr++ = orr_immed(cc, cc, 2, 29);
            break;

        case 16:
            *ptr++ = orr_immed(cc, cc, 1, 28);
            break;

        default:
            tmp_reg = RA_AllocARMRegister(&ptr);
            *ptr++ = mov_immed_u16(tmp_reg, flags, 0);
            *ptr++ = orr_reg(cc, cc, tmp_reg, LSL, 0);
            RA_FreeARMRegister(&ptr, tmp_reg);
    }

    return ptr;
}

#if 0
static inline __attribute__((always_inline))
uint32_t * EMIT_SetFlagsConditional(uint32_t * ptr, uint8_t cc, uint8_t flags, uint8_t cond)
{
    uint8_t tmp_reg = RA_AllocARMRegister(&ptr);

    switch (flags)
    {
        case 0:
            break;

        case 1:
            *ptr++ = cset(tmp_reg, cond);
            *ptr++ = orr_reg(cc, cc, tmp_reg, LSL, 0);
            break;

        case 2:
            *ptr++ = cset(tmp_reg, cond);
            *ptr++ = orr_reg(cc, cc, tmp_reg, LSL, 1);
            break;

        case 4:
            *ptr++ = cset(tmp_reg, cond);
            *ptr++ = orr_reg(cc, cc, tmp_reg, LSL, 2);
            break;

        case 8:
            *ptr++ = cset(tmp_reg, cond);
            *ptr++ = orr_reg(cc, cc, tmp_reg, LSL, 3);
            break;

        case 16:
            *ptr++ = cset(tmp_reg, cond);
            *ptr++ = orr_reg(cc, cc, tmp_reg, LSL, 4);
            break;

        default:
            *ptr++ = mov_immed_u16(tmp_reg, flags, 0);
            *ptr++ = csel(tmp_reg, tmp_reg, 31, cond);
            *ptr++ = orr_reg(cc, cc, tmp_reg, LSL, 0);
    }

    RA_FreeARMRegister(&ptr, tmp_reg);
    return ptr;
}
#else
static inline __attribute__((always_inline))
uint32_t * EMIT_SetFlagsConditional(uint32_t * ptr, uint8_t cc, uint8_t flags, uint8_t cond)
{
    uint8_t tmp_reg = RA_AllocARMRegister(&ptr);

    switch (flags)
    {
        case 0:
            break;

        case 1:
            *ptr++ = b_cc(cond ^ 1, 2);
            *ptr++ = orr_immed(cc, cc, 1, 0);
            break;

        case 2:
            *ptr++ = b_cc(cond ^ 1, 2);
            *ptr++ = orr_immed(cc, cc, 1, 31);
            break;

        case 4:
            *ptr++ = b_cc(cond ^ 1, 2);
            *ptr++ = orr_immed(cc, cc, 1, 30);
            break;

        case 8:
            *ptr++ = b_cc(cond ^ 1, 2);
            *ptr++ = orr_immed(cc, cc, 1, 29);
            break;

        case 16:
            *ptr++ = b_cc(cond ^ 1, 2);
            *ptr++ = orr_immed(cc, cc, 1, 28);
            break;

        default:
            *ptr++ = b_cc(cond ^ 1, 3);
            *ptr++ = mov_immed_u16(tmp_reg, flags, 0);
            *ptr++ = orr_reg(cc, cc, tmp_reg, LSL, 0);
    }

    RA_FreeARMRegister(&ptr, tmp_reg);
    return ptr;
}
#endif

#endif /* _A64_H */
