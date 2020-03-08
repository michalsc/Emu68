/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef _ARM_H
#define _ARM_H

#include "support.h"

/*
    Global registers:

    r0 - r9 - Free for emulator use

    r10 - Status Register of m68k
    r11 - the m68k context
    r12 - the Program Counter of m68k code
*/

#define REG_PC 12
#define REG_CTX 11
#define REG_SR 10

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

/* Converts generated ARM instruction to little-endian */
static inline uint32_t INSN_TO_LE(uint32_t insn)
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

static inline uint32_t adc_cc_immed(uint8_t cc, uint8_t dest, uint16_t src, uint16_t value){ dest = dest & 15; src = src & 15; return INSN_TO_LE(0x02a00000 | (cc << 28) | (dest << 12) | (src << 16) | value);}
static inline uint32_t adc_immed(uint8_t dest, uint8_t src, uint16_t value) { return adc_cc_immed(ARM_CC_AL, dest, src, value); }
static inline uint32_t adc_cc_reg(uint8_t cc, uint8_t dest, uint8_t src, uint8_t reg, uint8_t lsl){dest = dest & 15;src = src & 15;reg = reg & 15;lsl = lsl & 31;return INSN_TO_LE(0x00a00000 | (cc << 28) | (dest << 12) | (src << 16) | reg | (lsl << 7));}
static inline uint32_t adc_reg(uint8_t dest, uint8_t src, uint8_t reg, uint8_t lsl){return adc_cc_reg(ARM_CC_AL, dest, src, reg, lsl);}
static inline uint32_t adcs_cc_reg(uint8_t cc, uint8_t dest, uint8_t src, uint8_t reg, uint8_t lsl){dest = dest & 15;src = src & 15;reg = reg & 15;lsl = lsl & 31;return INSN_TO_LE(0x00a00000 | (1 << 20) | (cc << 28) | (dest << 12) | (src << 16) | reg | (lsl << 7));}
static inline uint32_t adcs_reg(uint8_t dest, uint8_t src, uint8_t reg, uint8_t lsl){return adcs_cc_reg(ARM_CC_AL, dest, src, reg, lsl);}
static inline uint32_t adcs_cc_immed(uint8_t cc, uint8_t dest, uint8_t src, uint16_t value){dest = dest & 15;src = src & 15;return INSN_TO_LE(0x02a00000 | (1 << 20) | (cc << 28) | (dest << 12) | (src << 16) | value);}
static inline uint32_t adcs_immed(uint8_t dest, uint8_t src, uint16_t value){return adcs_cc_immed(ARM_CC_AL, dest, src, value);}
static inline uint32_t add_cc_immed(uint8_t cc, uint8_t dest, uint16_t src, uint16_t value){ dest = dest & 15; src = src & 15; return INSN_TO_LE(0x02800000 | (cc << 28) | (dest << 12) | (src << 16) | value);}
static inline uint32_t add_immed(uint8_t dest, uint8_t src, uint16_t value) { return add_cc_immed(ARM_CC_AL, dest, src, value); }
static inline uint32_t add_cc_reg(uint8_t cc, uint8_t dest, uint8_t src, uint8_t reg, uint8_t lsl){dest = dest & 15;src = src & 15;reg = reg & 15;lsl = lsl & 31;return INSN_TO_LE(0x00800000 | (cc << 28) | (dest << 12) | (src << 16) | reg | (lsl << 7));}
static inline uint32_t add_reg(uint8_t dest, uint8_t src, uint8_t reg, uint8_t lsl){return add_cc_reg(ARM_CC_AL, dest, src, reg, lsl);}
static inline uint32_t add_cc_reg_lsr_imm(uint8_t cc, uint8_t dest, uint8_t src, uint8_t reg, uint8_t lsr){dest = dest & 15;src = src & 15;reg = reg & 15;lsr = lsr & 31;return INSN_TO_LE(0x00800020 | (cc << 28) | (dest << 12) | (src << 16) | reg | (lsr << 7));}
static inline uint32_t add_reg_lsr_imm(uint8_t dest, uint8_t src, uint8_t reg, uint8_t lsr){return add_cc_reg_lsr_imm(ARM_CC_AL, dest, src, reg, lsr);}
static inline uint32_t adds_cc_reg(uint8_t cc, uint8_t dest, uint8_t src, uint8_t reg, uint8_t lsl){dest = dest & 15;src = src & 15;reg = reg & 15;lsl = lsl & 31;return INSN_TO_LE(0x00800000 | (1 << 20) | (cc << 28) | (dest << 12) | (src << 16) | reg | (lsl << 7));}
static inline uint32_t adds_reg(uint8_t dest, uint8_t src, uint8_t reg, uint8_t lsl){return adds_cc_reg(ARM_CC_AL, dest, src, reg, lsl);}
static inline uint32_t adds_cc_immed(uint8_t cc, uint8_t dest, uint8_t src, uint16_t value){dest = dest & 15;src = src & 15;return INSN_TO_LE(0x02800000 | (1 << 20) | (cc << 28) | (dest << 12) | (src << 16) | value);}
static inline uint32_t adds_immed(uint8_t dest, uint8_t src, uint16_t value){return adds_cc_immed(ARM_CC_AL, dest, src, value);}
static inline uint32_t and_cc_immed(uint8_t cc, uint8_t dest, uint8_t src, uint8_t value){return INSN_TO_LE(0x02000000 | (cc << 28) | (dest << 12) | (src << 16) | value);}
static inline uint32_t and_immed(uint8_t dest, uint8_t src, uint8_t value){return and_cc_immed(ARM_CC_AL, dest, src, value);}
static inline uint32_t and_cc_reg(uint8_t cc, uint8_t dest, uint8_t src, uint8_t reg, uint8_t lsl){return INSN_TO_LE(0x00000000 | (cc << 28) | (dest << 12) | (src << 16) | reg | (lsl << 7));}
static inline uint32_t and_reg(uint8_t dest, uint8_t src, uint8_t reg, uint8_t lsl){return and_cc_reg(ARM_CC_AL, dest, src, reg, lsl);}
static inline uint32_t ands_cc_immed(uint8_t cc, uint8_t dest, uint8_t src, uint8_t value){return INSN_TO_LE(0x02000000 | (cc << 28) | (1 << 20) | (dest << 12) | (src << 16) | value);}
static inline uint32_t ands_immed(uint8_t dest, uint8_t src, uint8_t value){return ands_cc_immed(ARM_CC_AL, dest, src, value);}
static inline uint32_t ands_cc_reg(uint8_t cc, uint8_t dest, uint8_t src, uint8_t reg, uint8_t lsl){return INSN_TO_LE(0x00000000 | (cc << 28) | (1 << 20) | (dest << 12) | (src << 16) | reg | (lsl << 7));}
static inline uint32_t ands_reg(uint8_t dest, uint8_t src, uint8_t reg, uint8_t lsl){return ands_cc_reg(ARM_CC_AL, dest, src, reg, lsl);}
static inline uint32_t asr_cc_immed(uint8_t cc, uint8_t dest, uint8_t src, uint8_t value){return INSN_TO_LE(0x01a00040 | (cc << 28) | (dest << 12) | src | ((value & 0x1f) << 7));}
static inline uint32_t asr_immed(uint8_t dest, uint8_t src, uint8_t value){return asr_cc_immed(ARM_CC_AL, dest, src, value);}
static inline uint32_t asr_cc_reg(uint8_t cc, uint8_t dest, uint8_t src, uint8_t value){return INSN_TO_LE(0x01a00050 | (cc << 28) | (dest << 12) | src | ((value & 0xf) << 8));}
static inline uint32_t asr_reg(uint8_t dest, uint8_t src, uint8_t value){return asr_cc_reg(ARM_CC_AL, dest, src, value);}
static inline uint32_t asrs_cc_immed(uint8_t cc, uint8_t dest, uint8_t src, uint8_t value){return INSN_TO_LE(0x01a00040 | (1 << 20) | (cc << 28) | (dest << 12) | src | ((value & 0x1f) << 7));}
static inline uint32_t asrs_immed(uint8_t dest, uint8_t src, uint8_t value){return asrs_cc_immed(ARM_CC_AL, dest, src, value);}
static inline uint32_t asrs_cc_reg(uint8_t cc, uint8_t dest, uint8_t src, uint8_t value){return INSN_TO_LE(0x01a00050 | (1 << 20) | (cc << 28) | (dest << 12) | src | ((value & 0xf) << 8));}
static inline uint32_t asrs_reg(uint8_t dest, uint8_t src, uint8_t value){return asrs_cc_reg(ARM_CC_AL, dest, src, value);}
static inline uint32_t bic_cc_immed(uint8_t cc, uint8_t dest, uint8_t src, uint16_t mask) { dest = dest & 15; src = src & 15; return INSN_TO_LE(0x03c00000 | (cc << 28) | mask | (dest << 12) | (src << 16));}
static inline uint32_t bic_immed(uint8_t dest, uint8_t src, uint16_t mask) { return bic_cc_immed(ARM_CC_AL, dest, src, mask);}
static inline uint32_t bic_cc_reg(uint8_t cc, uint8_t dest, uint8_t src, uint8_t reg) { dest = dest & 15; src = src & 15; return INSN_TO_LE(0x01c00000 | (cc << 28) | reg | (dest << 12) | (src << 16));}
static inline uint32_t bic_reg(uint8_t dest, uint8_t src, uint8_t reg) { return bic_cc_reg(ARM_CC_AL, dest, src, reg);}
static inline uint32_t b_cc(uint8_t cc, int32_t offset) { return INSN_TO_LE(0x0a000000 | (cc << 28) | (offset & 0x00ffffff));}
static inline uint32_t bl_cc(uint8_t cc, int32_t offset) { return INSN_TO_LE(0x0b000000 | (cc << 28) | (offset & 0x00ffffff));}
static inline uint32_t bfc_cc(uint8_t cc, uint8_t reg, uint8_t lsb, uint8_t width) { return INSN_TO_LE(0x07c0001f | (cc << 28) | (reg << 12) | (lsb << 7) | ((lsb + width - 1) << 16)); }
static inline uint32_t bfc(uint8_t reg, uint8_t lsb, uint8_t width) { return bfc_cc(ARM_CC_AL, reg, lsb, width); }
static inline uint32_t bfi_cc(uint8_t cc, uint8_t reg, uint8_t src, uint8_t lsb, uint8_t width) { return INSN_TO_LE(0x07c00010 | (cc << 28) | (reg << 12) | (lsb << 7) | ((lsb + width - 1) << 16) | src); }
static inline uint32_t bfi(uint8_t reg, uint8_t src, uint8_t lsb, uint8_t width) { return bfi_cc(ARM_CC_AL, reg, src, lsb, width); }
static inline uint32_t blx_cc_reg(uint8_t cc, uint8_t reg) { return (INSN_TO_LE(0x012fff30 | (cc << 28) | reg));}
static inline uint32_t bx_lr() { return INSN_TO_LE(0xe12fff1e); }
static inline uint32_t clz_cc(uint8_t cc, uint8_t rd, uint8_t rm) { return INSN_TO_LE(0x016f0f10 | (cc << 28) | (rd << 12) | rm);}
static inline uint32_t clz(uint8_t rd, uint8_t rm) { return clz_cc(ARM_CC_AL, rd, rm);}
static inline uint32_t cmp_cc_immed(uint8_t cc, uint8_t src, uint16_t value) { src = src & 15; return INSN_TO_LE(0x03500000 | (cc << 28) | (src << 16) | value); }
static inline uint32_t cmp_immed(uint8_t src, uint16_t value) { return cmp_cc_immed(ARM_CC_AL, src, value); }
static inline uint32_t cmp_cc_reg(uint8_t cc, uint8_t src, uint8_t value) { src = src & 15; return INSN_TO_LE(0x01500000 | (cc << 28) | (src << 16) | value); }
static inline uint32_t cmp_reg(uint8_t src, uint8_t value) { return cmp_cc_reg(ARM_CC_AL, src, value); }
static inline uint32_t cmn_cc_immed(uint8_t cc, uint8_t src, uint16_t value) { src = src & 15; return INSN_TO_LE(0x03700000 | (cc << 28) | (src << 16) | value); }
static inline uint32_t cmn_immed(uint8_t src, uint16_t value) { return cmn_cc_immed(ARM_CC_AL, src, value); }
static inline uint32_t cmn_cc_reg(uint8_t cc, uint8_t src, uint8_t value) { src = src & 15; return INSN_TO_LE(0x01700000 | (cc << 28) | (src << 16) | value); }
static inline uint32_t cmn_reg(uint8_t src, uint8_t value) { return cmn_cc_reg(ARM_CC_AL, src, value); }
static inline uint32_t eor_cc_immed(uint8_t cc, uint8_t dest, uint8_t src, uint16_t value){return INSN_TO_LE(0x02200000 | (cc << 28) | (dest << 12) | (src << 16) | value);}
static inline uint32_t eor_immed(uint8_t dest, uint8_t src, uint16_t value){return eor_cc_immed(ARM_CC_AL, dest, src, value);}
static inline uint32_t eor_cc_reg(uint8_t cc, uint8_t dest, uint8_t src, uint8_t reg, uint8_t lsl){return INSN_TO_LE(0x00200000 | (cc << 28) | (dest << 12) | (src << 16) | reg | (lsl << 7));}
static inline uint32_t eor_reg(uint8_t dest, uint8_t src, uint8_t reg, uint8_t lsl){return eor_cc_reg(ARM_CC_AL, dest, src, reg, lsl);}
static inline uint32_t eors_cc_immed(uint8_t cc, uint8_t dest, uint8_t src, uint16_t value){return INSN_TO_LE(0x02200000 | (cc << 28) | (1 << 20) | (dest << 12) | (src << 16) | value);}
static inline uint32_t eors_immed(uint8_t dest, uint8_t src, uint16_t value){return eors_cc_immed(ARM_CC_AL, dest, src, value);}
static inline uint32_t eors_cc_reg(uint8_t cc, uint8_t dest, uint8_t src, uint8_t reg, uint8_t lsl){return INSN_TO_LE(0x00200000 | (cc << 28) | (1 << 20) | (dest << 12) | (src << 16) | reg | (lsl << 7));}
static inline uint32_t eors_reg(uint8_t dest, uint8_t src, uint8_t reg, uint8_t lsl){return eors_cc_reg(ARM_CC_AL, dest, src, reg, lsl);}
static inline uint32_t ldrex_cc(uint8_t cc, uint8_t rn, uint8_t rd)  { return INSN_TO_LE(0x01900f9f | (cc << 28) | (rn << 16) | (rd << 12)); }
static inline uint32_t ldrex(uint8_t rn, uint8_t rd) { return ldrex_cc(ARM_CC_AL, rn, rd); }
static inline uint32_t ldrexb_cc(uint8_t cc, uint8_t rn, uint8_t rd) { return INSN_TO_LE(0x01d00f9f | (cc << 28) | (rn << 16) | (rd << 12)); }
static inline uint32_t ldrexb(uint8_t rn, uint8_t rd) { return ldrexb_cc(ARM_CC_AL, rn, rd); }
static inline uint32_t ldrexh_cc(uint8_t cc, uint8_t rn, uint8_t rd) { return INSN_TO_LE(0x01f00f9f | (cc << 28) | (rn << 16) | (rd << 12)); }
static inline uint32_t ldrexh(uint8_t rn, uint8_t rd) { return ldrexh_cc(ARM_CC_AL, rn, rd); }
static inline uint32_t ldr_cc_offset(uint8_t cc, uint8_t dest, uint8_t src, int16_t offset){return (offset >= 0) ? INSN_TO_LE(0x05900000 | (cc << 28) | (dest << 16) | (src << 12) | offset) : INSN_TO_LE(0x05100000 | (cc << 28) | (dest << 16) | (src << 12) | -offset);}
static inline uint32_t ldr_offset(uint8_t dest, uint8_t src, int16_t offset){return ldr_cc_offset(ARM_CC_AL, dest, src, offset);}
static inline uint32_t ldr_cc_offset_preindex(uint8_t cc, uint8_t dest, uint8_t src, int16_t offset){return (offset >= 0) ? INSN_TO_LE(0x05b00000 | (cc << 28) | (dest << 16) | (src << 12) | offset) : INSN_TO_LE(0x05300000 | (cc << 28) | (dest << 16) | (src << 12) | -offset);}
static inline uint32_t ldr_offset_preindex(uint8_t dest, uint8_t src, int16_t offset){return ldr_cc_offset_preindex(ARM_CC_AL, dest, src, offset);}
static inline uint32_t ldr_cc_offset_postindex(uint8_t cc, uint8_t dest, uint8_t src, int16_t offset){return (offset >= 0) ? INSN_TO_LE(0x04900000 | (cc << 28) | (dest << 16) | (src << 12) | offset) : INSN_TO_LE(0x04100000 | (cc << 28) | (dest << 16) | (src << 12) | -offset);}
static inline uint32_t ldr_offset_postindex(uint8_t dest, uint8_t src, int16_t offset){return ldr_cc_offset_postindex(ARM_CC_AL, dest, src, offset);}
static inline uint32_t ldr_cc_regoffset(uint8_t cc, uint8_t dest, uint8_t src, uint8_t reg, uint8_t shift){return INSN_TO_LE(0x07900000 | (cc << 28) | (dest << 16) | (src << 12) | reg | ((shift & 0x1f) << 7));}
static inline uint32_t ldr_regoffset(uint8_t dest, uint8_t src, uint8_t reg, uint8_t shift){return ldr_cc_regoffset(ARM_CC_AL, dest, src, reg, shift);}
static inline uint32_t ldrh_cc_offset(uint8_t cc, uint8_t dest, uint8_t src, int16_t offset){return (offset >= 0) ? INSN_TO_LE(0x01d000b0 | (cc << 28) | (dest << 16) | (src << 12) | (offset & 0x0f) | ((offset << 4) & 0xf00)) : INSN_TO_LE(0x015000b0 | (cc << 28) | (dest << 16) | (src << 12) | ((-offset) & 0x0f) | (((-offset) << 4) & 0xf00));}
static inline uint32_t ldrh_offset(uint8_t dest, uint8_t src, int16_t offset){return ldrh_cc_offset(ARM_CC_AL, dest, src, offset);}
static inline uint32_t ldrh_cc_offset_preindex(uint8_t cc, uint8_t dest, uint8_t src, int8_t offset){return (offset >= 0) ? INSN_TO_LE(0x01f000b0 | (cc << 28) | (dest << 16) | (src << 12) | (offset & 0x0f) | ((offset << 4) & 0xf00)) : INSN_TO_LE(0x017000b0 | (cc << 28) | (dest << 16) | (src << 12) | ((-offset) & 0x0f) | (((-offset) << 4) & 0xf00));}
static inline uint32_t ldrh_offset_preindex(uint8_t dest, uint8_t src, int8_t offset){return ldrh_cc_offset_preindex(ARM_CC_AL, dest, src, offset);}
static inline uint32_t ldrh_cc_offset_postindex(uint8_t cc, uint8_t dest, uint8_t src, int8_t offset){return (offset >= 0) ? INSN_TO_LE(0x00d000b0 | (cc << 28) | (dest << 16) | (src << 12) | (offset & 0x0f) | ((offset << 4) & 0xf00)) : INSN_TO_LE(0x005000b0 | (cc << 28) | (dest << 16) | (src << 12) | ((-offset) & 0x0f) | (((-offset << 4)) & 0xf00));}
static inline uint32_t ldrh_offset_postindex(uint8_t dest, uint8_t src, int8_t offset){return ldrh_cc_offset_postindex(ARM_CC_AL, dest, src, offset);}
static inline uint32_t ldrh_cc_regoffset(uint8_t cc, uint8_t dest, uint8_t src, uint8_t offset){return INSN_TO_LE(0x019000b0 | (cc << 28) | (dest << 16) | (src << 12) | (offset & 0x0f));}
static inline uint32_t ldrh_regoffset(uint8_t dest, uint8_t src, uint8_t offset){return ldrh_cc_regoffset(ARM_CC_AL, dest, src, offset);}
static inline uint32_t ldrsh_cc_offset(uint8_t cc, uint8_t dest, uint8_t src, int8_t offset){return (offset >= 0) ? INSN_TO_LE(0x01d000f0 | (cc << 28) | (dest << 16) | (src << 12) | (offset & 0x0f) | ((offset << 4) & 0xf00)) : INSN_TO_LE(0x015000f0 | (cc << 28) | (dest << 16) | (src << 12) | (-offset & 0x0f) | ((-offset << 4) & 0xf00));}
static inline uint32_t ldrsh_offset(uint8_t dest, uint8_t src, int8_t offset){return ldrsh_cc_offset(ARM_CC_AL, dest, src, offset);}
static inline uint32_t ldrb_cc_offset(uint8_t cc, uint8_t dest, uint8_t src, int16_t offset){return (offset >= 0) ? INSN_TO_LE(0x05d00000 | (cc << 28) | (dest << 16) | (src << 12) | offset) : INSN_TO_LE(0x05500000 | (cc << 28) | (dest << 16) | (src << 12) | -offset);}
static inline uint32_t ldrb_offset(uint8_t dest, uint8_t src, int16_t offset){return ldrb_cc_offset(ARM_CC_AL, dest, src, offset);}
static inline uint32_t ldrb_cc_offset_preindex(uint8_t cc, uint8_t dest, uint8_t src, int16_t offset){return (offset >= 0) ? INSN_TO_LE(0x05f00000 | (cc << 28) | (dest << 16) | (src << 12) | offset) : INSN_TO_LE(0x05700000 | (cc << 28) | (dest << 16) | (src << 12) | -offset);}
static inline uint32_t ldrb_offset_preindex(uint8_t dest, uint8_t src, int16_t offset){return ldrb_cc_offset_preindex(ARM_CC_AL, dest, src, offset);}
static inline uint32_t ldrb_cc_offset_postindex(uint8_t cc, uint8_t dest, uint8_t src, int16_t offset){return (offset >= 0) ? INSN_TO_LE(0x04d00000 | (cc << 28) | (dest << 16) | (src << 12) | offset) : INSN_TO_LE(0x04500000 | (cc << 28) | (dest << 16) | (src << 12) | -offset);}
static inline uint32_t ldrb_offset_postindex(uint8_t dest, uint8_t src, int16_t offset){return ldrb_cc_offset_postindex(ARM_CC_AL, dest, src, offset);}
static inline uint32_t ldrb_cc_regoffset(uint8_t cc, uint8_t dest, uint8_t src, uint8_t offset, uint8_t shift){return INSN_TO_LE(0x07d00000 | (cc << 28) | (dest << 16) | (src << 12) | offset | ((shift & 0x1f) << 7));}
static inline uint32_t ldrb_regoffset(uint8_t dest, uint8_t src, uint8_t offset, uint8_t shift){return ldrb_cc_regoffset(ARM_CC_AL, dest, src, offset, shift);}
static inline uint32_t ldrsb_cc_offset(uint8_t cc, uint8_t dest, uint8_t src, int8_t offset){return (offset >= 0) ? INSN_TO_LE(0x01d000d0 | (cc << 28) | (dest << 16) | (src << 12) | (offset & 0x0f) | ((offset << 4) & 0xf00)) : INSN_TO_LE(0x015000f0 | (cc << 28) | (dest << 16) | (src << 12) | (-offset & 0x0f) | ((-offset << 4) & 0xf00));}
static inline uint32_t ldrsb_offset(uint8_t dest, uint8_t src, int8_t offset){return ldrsb_cc_offset(ARM_CC_AL, dest, src, offset);}
static inline uint32_t lsl_cc_immed(uint8_t cc, uint8_t dest, uint8_t src, uint8_t value){return INSN_TO_LE(0x01a00000 | (cc << 28) | (dest << 12) | src | ((value & 0x1f) << 7));}
static inline uint32_t lsl_immed(uint8_t dest, uint8_t src, uint8_t value){return lsl_cc_immed(ARM_CC_AL, dest, src, value);}
static inline uint32_t lsl_cc_reg(uint8_t cc, uint8_t dest, uint8_t src, uint8_t value){return INSN_TO_LE(0x01a00010 | (cc << 28) | (dest << 12) | src | ((value & 0xf) << 8));}
static inline uint32_t lsl_reg(uint8_t dest, uint8_t src, uint8_t value){return lsl_cc_reg(ARM_CC_AL, dest, src, value);}
static inline uint32_t lsls_cc_immed(uint8_t cc, uint8_t dest, uint8_t src, uint8_t value) { return INSN_TO_LE(0x01a00000 | (1 << 20) | (cc << 28) | (dest << 12) | src | ((value & 0x1f) << 7)); }
static inline uint32_t lsls_immed(uint8_t dest, uint8_t src, uint8_t value) { return lsls_cc_immed(ARM_CC_AL, dest, src, value); }
static inline uint32_t lsls_cc_reg(uint8_t cc, uint8_t dest, uint8_t src, uint8_t value) { return INSN_TO_LE(0x01a00010 | (1 << 20) | (cc << 28) | (dest << 12) | src | ((value & 0xf) << 8)); }
static inline uint32_t lsls_reg(uint8_t dest, uint8_t src, uint8_t value) { return lsls_cc_reg(ARM_CC_AL, dest, src, value); }
static inline uint32_t lsr_cc_immed(uint8_t cc, uint8_t dest, uint8_t src, uint8_t value){return INSN_TO_LE(0x01a00020 | (cc << 28) | (dest << 12) | src | ((value & 0x1f) << 7));}
static inline uint32_t lsr_immed(uint8_t dest, uint8_t src, uint8_t value){return lsr_cc_immed(ARM_CC_AL, dest, src, value);}
static inline uint32_t lsr_cc_reg(uint8_t cc, uint8_t dest, uint8_t src, uint8_t value){return INSN_TO_LE(0x01a00030 | (cc << 28) | (dest << 12) | src | ((value & 0xf) << 8));}
static inline uint32_t lsr_reg(uint8_t dest, uint8_t src, uint8_t value){return lsr_cc_reg(ARM_CC_AL, dest, src, value);}
static inline uint32_t lsrs_cc_immed(uint8_t cc, uint8_t dest, uint8_t src, uint8_t value){return INSN_TO_LE(0x01a00020 | (1 << 20) | (cc << 28) | (dest << 12) | src | ((value & 0x1f) << 7));}
static inline uint32_t lsrs_immed(uint8_t dest, uint8_t src, uint8_t value){return lsrs_cc_immed(ARM_CC_AL, dest, src, value);}
static inline uint32_t lsrs_cc_reg(uint8_t cc, uint8_t dest, uint8_t src, uint8_t value){return INSN_TO_LE(0x01a00030 | (1 << 20) | (cc << 28) | (dest << 12) | src | ((value & 0xf) << 8));}
static inline uint32_t lsrs_reg(uint8_t dest, uint8_t src, uint8_t value){return lsrs_cc_reg(ARM_CC_AL, dest, src, value);}
static inline uint32_t mcr(uint8_t cp, uint8_t op1, uint8_t rd, uint8_t crn, uint8_t crm, uint8_t op2) { return INSN_TO_LE(0xee000010 | (op1 << 21) | (crn << 16) | (rd << 12) | (cp << 8) | (op2 << 5) | crm); }
static inline uint32_t mrc(uint8_t cp, uint8_t op1, uint8_t rd, uint8_t crn, uint8_t crm, uint8_t op2) { return INSN_TO_LE(0xee100010 | (op1 << 21) | (crn << 16) | (rd << 12) | (cp << 8) | (op2 << 5) | crm); }
static inline uint32_t mov_cc_reg(uint8_t cc, uint8_t reg, uint8_t src) { return INSN_TO_LE(0x01a00000 | (cc << 28) | src | (reg << 12)); }
static inline uint32_t mov_reg(uint8_t reg, uint8_t src) { return INSN_TO_LE(0xe1a00000 | src | (reg << 12)); }
static inline uint32_t mov_cc_reg_shift(uint8_t cc, uint8_t reg, uint8_t src, uint8_t shift) { return INSN_TO_LE(0x01a00000 | (cc << 28) | src | (reg << 12) | (shift << 7)); }
static inline uint32_t mov_reg_shift(uint8_t reg, uint8_t src, uint8_t shift) { return INSN_TO_LE(0xe1a00000 | src | (reg << 12) | (shift << 7)); }
static inline uint32_t movs_reg_shift(uint8_t reg, uint8_t src, uint8_t shift) { return INSN_TO_LE(0xe1a00000 | src | (reg << 12) | (shift << 7) | (1 << 20)); }
static inline uint32_t mov_cc_immed_u8(uint8_t cc, uint8_t reg, uint8_t val) { reg = reg & 0x0f; return INSN_TO_LE(0x03a00000 | (cc << 28) | val | (reg << 12)); }
static inline uint32_t movs_cc_immed_u8(uint8_t cc, uint8_t reg, uint8_t val) { reg = reg & 0x0f; return INSN_TO_LE(0x03a00000 | (cc << 28) | val | (reg << 12) | (1 << 20)); }
static inline uint32_t mov_immed_u8(uint8_t reg, uint8_t val) { return mov_cc_immed_u8(ARM_CC_AL, reg, val); }
static inline uint32_t movs_immed_u8(uint8_t reg, uint8_t val) { return movs_cc_immed_u8(ARM_CC_AL, reg, val); }
static inline uint32_t mov_immed_u8_shift(uint8_t reg, uint8_t val, uint8_t shift) { reg = reg & 0x0f; shift &= 0x0f; return INSN_TO_LE(0xe3a00000 | val | (reg << 12) | (shift << 8)); }
static inline uint32_t movs_immed_u8_shift(uint8_t reg, uint8_t val, uint8_t shift) { reg = reg & 0x0f; shift &= 0x0f; return INSN_TO_LE(0xe3a00000 | val | (reg << 12) | (1 << 20) | (shift << 8)); }
static inline uint32_t movt_cc_immed_u16(uint8_t cc, uint8_t reg, uint16_t val) { reg = reg & 0x0f; uint8_t imm4 = val >> 12; uint16_t imm12 = val &0xfff; return INSN_TO_LE(0x03400000 | (cc << 28) | (imm4 << 16) | (imm12) | (reg << 12)); }
static inline uint32_t movt_immed_u16(uint8_t reg, uint16_t val) { return movt_cc_immed_u16(ARM_CC_AL, reg, val); }
static inline uint32_t movw_cc_immed_u16(uint8_t cc, uint8_t reg, uint16_t val) { reg = reg & 0x0f; uint8_t imm4 = val >> 12; uint16_t imm12 = val &0xfff; return INSN_TO_LE(0x03000000 | (cc << 28) | (imm4 << 16) | (imm12) | (reg << 12)); }
static inline uint32_t movw_immed_u16(uint8_t reg, uint16_t val) { return movw_cc_immed_u16(ARM_CC_AL, reg, val); }
static inline uint32_t mrs(uint8_t reg) { return INSN_TO_LE(0xe10f0000 | (reg << 12)); }
static inline uint32_t msr(uint8_t reg, uint8_t mask) { return INSN_TO_LE(0xe120f000 | (reg) | (mask << 16)); }
static inline uint32_t mul_cc(uint8_t cc, uint8_t rd, uint8_t rm, uint8_t rs) { return INSN_TO_LE(0x00000090 | (cc << 28) | (rd << 16) | (rs << 8) | rm);}
static inline uint32_t mul(uint8_t rd, uint8_t rm, uint8_t rs) { return mul_cc(ARM_CC_AL, rd, rm, rs);}
static inline uint32_t muls_cc(uint8_t cc, uint8_t rd, uint8_t rm, uint8_t rs) { return INSN_TO_LE(0x00000090 | (1 << 20) | (cc << 28) | (rd << 16) | (rs << 8) | rm);}
static inline uint32_t muls(uint8_t rd, uint8_t rm, uint8_t rs) { return muls_cc(ARM_CC_AL, rd, rm, rs);}
static inline uint32_t mvn_cc_immed_u8(uint8_t cc, uint8_t reg, uint8_t val) { reg = reg & 0x0f; return INSN_TO_LE(0x03e00000 | (cc << 28) | val | (reg << 12)); }
static inline uint32_t mvns_cc_immed_u8(uint8_t cc, uint8_t reg, uint8_t val) { reg = reg & 0x0f; return INSN_TO_LE(0x03e00000 | (cc << 28) | val | (reg << 12) | (1 << 20)); }
static inline uint32_t mvn_immed_u8(uint8_t reg, uint8_t val) { return mvn_cc_immed_u8(ARM_CC_AL, reg, val); }
static inline uint32_t mvns_immed_u8(uint8_t reg, uint8_t val) { return mvns_cc_immed_u8(ARM_CC_AL, reg, val); }
static inline uint32_t mov_cc_immed_s8(uint8_t cc, uint8_t reg, int8_t val) { reg = reg & 0x0f; return (val >= 0) ? mov_cc_immed_u8(cc, reg, (uint8_t)val) : mvn_cc_immed_u8(cc, reg, (uint8_t)(-val - 1)); }
static inline uint32_t mov_immed_s8(uint8_t reg, int8_t val) { reg = reg & 0x0f; return (val >= 0) ? mov_immed_u8(reg, (uint8_t)val) : mvn_immed_u8(reg, (uint8_t)(-val - 1)); }
static inline uint32_t movs_immed_s8(uint8_t reg, int8_t val) { reg = reg & 0x0f; return (val >= 0) ? movs_immed_u8(reg, (uint8_t)val) : mvns_immed_u8(reg, (uint8_t)(-val - 1)); }
static inline uint32_t mvn_immed_u8_shift(uint8_t reg, uint8_t val, uint8_t shift) { reg = reg & 0x0f; shift &= 0x0f; return INSN_TO_LE(0xe3e00000 | val | (reg << 12) | (shift << 8)); }
static inline uint32_t mvns_immed_u8_shift(uint8_t reg, uint8_t val, uint8_t shift) { reg = reg & 0x0f; shift &= 0x0f; return INSN_TO_LE(0xe3e00000 | val | (reg << 12) | (1 << 20) | (shift << 8)); }
static inline uint32_t mvn_reg(uint8_t dest, uint8_t src, uint8_t shift) { return INSN_TO_LE(0xe1e00000 | src | (dest << 12) | (shift << 8)); }
static inline uint32_t mvns_reg(uint8_t dest, uint8_t src, uint8_t shift) { return INSN_TO_LE(0xe1e00000 | src | (dest << 12) | (1 << 20) | (shift << 8)); }
static inline uint32_t orr_cc_immed(uint8_t cc, uint8_t dest, uint8_t src, uint16_t value){return INSN_TO_LE(0x03800000 | (cc << 28) | (dest << 12) | (src << 16) | value);}
static inline uint32_t orr_immed(uint8_t dest, uint8_t src, uint16_t value){return orr_cc_immed(ARM_CC_AL, dest, src, value);}
static inline uint32_t orr_cc_reg(uint8_t cc, uint8_t dest, uint8_t src, uint8_t reg, uint8_t lsl){return INSN_TO_LE(0x01800000 | (cc << 28) | (dest << 12) | (src << 16) | reg | (lsl << 7));}
static inline uint32_t orr_reg(uint8_t dest, uint8_t src, uint8_t reg, uint8_t lsl){return orr_cc_reg(ARM_CC_AL, dest, src, reg, lsl);}
static inline uint32_t orr_cc_reg_lsl_reg(uint8_t cc, uint8_t dest, uint8_t src, uint8_t reg, uint8_t lsl){return INSN_TO_LE(0x01800010 | (cc << 28) | (dest << 12) | (src << 16) | reg | (lsl << 8));}
static inline uint32_t orr_reg_lsl_reg(uint8_t dest, uint8_t src, uint8_t reg, uint8_t lsl){return orr_cc_reg_lsl_reg(ARM_CC_AL, dest, src, reg, lsl);}
static inline uint32_t orr_cc_reg_lsr_reg(uint8_t cc, uint8_t dest, uint8_t src, uint8_t reg, uint8_t lsl){return INSN_TO_LE(0x01800030 | (cc << 28) | (dest << 12) | (src << 16) | reg | (lsl << 8));}
static inline uint32_t orr_reg_lsr_reg(uint8_t dest, uint8_t src, uint8_t reg, uint8_t lsl){return orr_cc_reg_lsr_reg(ARM_CC_AL, dest, src, reg, lsl);}
static inline uint32_t orrs_cc_immed(uint8_t cc, uint8_t dest, uint8_t src, uint8_t value){return INSN_TO_LE(0x03800000 | (cc << 28) | (1 << 20) | (dest << 12) | (src << 16) | value);}
static inline uint32_t orrs_immed(uint8_t dest, uint8_t src, uint8_t value){return orrs_cc_immed(ARM_CC_AL, dest, src, value);}
static inline uint32_t orrs_cc_reg(uint8_t cc, uint8_t dest, uint8_t src, uint8_t reg, uint8_t lsl){return INSN_TO_LE(0x01800000 | (cc << 28) | (1 << 20) | (dest << 12) | (src << 16) | reg | (lsl << 7));}
static inline uint32_t orrs_reg(uint8_t dest, uint8_t src, uint8_t reg, uint8_t lsl){return orrs_cc_reg(ARM_CC_AL, dest, src, reg, lsl);}
static inline uint32_t push(uint16_t registers) {return INSN_TO_LE(0xe92d0000 | registers);}
static inline uint32_t pop(uint16_t registers) { return INSN_TO_LE(0xe8bd0000 | registers); }
static inline uint32_t rev_cc(uint8_t cc, uint8_t dest, uint8_t src){return INSN_TO_LE(0x06bf0f30 | (cc << 28) | (dest << 12) | src);}
static inline uint32_t rev(uint8_t dest, uint8_t src){return rev_cc(ARM_CC_AL, dest, src);}
static inline uint32_t ror_cc_immed(uint8_t cc, uint8_t dest, uint8_t src, uint8_t value){return INSN_TO_LE(0x01a00060 | (cc << 28) | (dest << 12) | src | ((value & 0x1f) << 7));}
static inline uint32_t ror_immed(uint8_t dest, uint8_t src, uint8_t value){return ror_cc_immed(ARM_CC_AL, dest, src, value);}
static inline uint32_t ror_cc_reg(uint8_t cc, uint8_t dest, uint8_t src, uint8_t value){return INSN_TO_LE(0x01a00070 | (cc << 28) | (dest << 12) | src | ((value & 0xf) << 8));}
static inline uint32_t ror_reg(uint8_t dest, uint8_t src, uint8_t value){return ror_cc_reg(ARM_CC_AL, dest, src, value);}
static inline uint32_t rors_cc_immed(uint8_t cc, uint8_t dest, uint8_t src, uint8_t value){return INSN_TO_LE(0x01a00060 | (1 << 20) | (cc << 28) | (dest << 12) | src | ((value & 0x1f) << 7));}
static inline uint32_t rors_immed(uint8_t dest, uint8_t src, uint8_t value){return rors_cc_immed(ARM_CC_AL, dest, src, value);}
static inline uint32_t rors_cc_reg(uint8_t cc, uint8_t dest, uint8_t src, uint8_t value){return INSN_TO_LE(0x01a00070 | (cc << 28) | (1 << 20) | (dest << 12) | src | ((value & 0xf) << 8));}
static inline uint32_t rors_reg(uint8_t dest, uint8_t src, uint8_t value){return rors_cc_reg(ARM_CC_AL, dest, src, value);}
static inline uint32_t rsb_cc_immed(uint8_t cc, uint8_t dest, uint8_t src, uint8_t value){return INSN_TO_LE(0x02600000 | (cc << 28) | (dest << 12) | (src << 16) | value);}
static inline uint32_t rsb_immed(uint8_t dest, uint8_t src, uint8_t value){return rsb_cc_immed(ARM_CC_AL, dest, src, value);}
static inline uint32_t rsbs_cc_immed(uint8_t cc, uint8_t dest, uint8_t src, uint8_t value){return INSN_TO_LE(0x02600000 | (1 << 20) | (cc << 28) | (dest << 12) | (src << 16) | value);}
static inline uint32_t rsbs_immed(uint8_t dest, uint8_t src, uint8_t value){return rsbs_cc_immed(ARM_CC_AL, dest, src, value);}
static inline uint32_t rsb_cc_reg(uint8_t cc, uint8_t dest, uint8_t src, uint8_t reg, uint8_t lsl){dest = dest & 15;src = src & 15;reg = reg & 15;lsl = lsl & 31;return INSN_TO_LE(0x00600000 | (cc << 28) | (dest << 12) | (src << 16) | reg | (lsl << 7));}
static inline uint32_t rsb_reg(uint8_t dest, uint8_t src, uint8_t reg, uint8_t lsl){return rsb_cc_reg(ARM_CC_AL, dest, src, reg, lsl);}
static inline uint32_t rsbs_cc_reg(uint8_t cc, uint8_t dest, uint8_t src, uint8_t reg, uint8_t lsl){dest = dest & 15;src = src & 15;reg = reg & 15;lsl = lsl & 31;return INSN_TO_LE(0x00600000 | (1 << 20) | (cc << 28) | (dest << 12) | (src << 16) | reg | (lsl << 7));}
static inline uint32_t rsbs_reg(uint8_t dest, uint8_t src, uint8_t reg, uint8_t lsl){return rsbs_cc_reg(ARM_CC_AL, dest, src, reg, lsl);}
static inline uint32_t sbfx_cc(uint8_t cc, uint8_t reg, uint8_t src, uint8_t lsb, uint8_t width) { return INSN_TO_LE(0x07a00050 | (cc << 28) | (reg << 12) | (lsb << 7) | ((width - 1) << 16) | src); }
static inline uint32_t sbfx(uint8_t reg, uint8_t src, uint8_t lsb, uint8_t width) { return sbfx_cc(ARM_CC_AL, reg, src, lsb, width); }
static inline uint32_t setend_be() { return INSN_TO_LE(0xf1010200); }
static inline uint32_t setend_le() { return INSN_TO_LE(0xf1010000); }
static inline uint32_t smull_cc(uint8_t cc, uint8_t rdhi, uint8_t rdlo, uint8_t rm, uint8_t rs) { return INSN_TO_LE(0x00c00090 | (cc << 28) | (rdhi << 16) | (rdlo << 12) | (rs << 8) | rm );}
static inline uint32_t smull(uint8_t rdhi, uint8_t rdlo, uint8_t rm, uint8_t rs) { return smull_cc(ARM_CC_AL, rdhi, rdlo, rm, rs);}
static inline uint32_t smulls_cc(uint8_t cc, uint8_t rdhi, uint8_t rdlo, uint8_t rm, uint8_t rs) { return INSN_TO_LE(0x00c00090 | (1 << 20) | (cc << 28) | (rdhi << 16) | (rdlo << 12) | (rs << 8) | rm );}
static inline uint32_t smulls(uint8_t rdhi, uint8_t rdlo, uint8_t rm, uint8_t rs) { return smulls_cc(ARM_CC_AL, rdhi, rdlo, rm, rs);}
static inline uint32_t str_cc_offset(uint8_t cc, uint8_t dest, uint8_t src, int16_t offset){return (offset >= 0) ? INSN_TO_LE(0x05800000 | (cc << 28) | (dest << 16) | (src << 12) | offset) : INSN_TO_LE(0x05000000 | (cc << 28) | (dest << 16) | (src << 12) | -offset);}
static inline uint32_t str_offset(uint8_t dest, uint8_t src, int16_t offset){return str_cc_offset(ARM_CC_AL, dest, src, offset);}
static inline uint32_t str_cc_regoffset(uint8_t cc, uint8_t dest, uint8_t src, uint8_t reg, uint8_t shift){return INSN_TO_LE(0x07800000 | (cc << 28) | (dest << 16) | (src << 12) | reg | ((shift & 0x1f) << 7));}
static inline uint32_t str_regoffset(uint8_t dest, uint8_t src, uint8_t reg, uint8_t shift){return str_cc_regoffset(ARM_CC_AL, dest, src, reg, shift);}
static inline uint32_t str_cc_offset_preindex(uint8_t cc, uint8_t dest, uint8_t src, int16_t offset){return (offset >= 0) ? INSN_TO_LE(0x05a00000 | (cc << 28) | (dest << 16) | (src << 12) | offset) : INSN_TO_LE(0x05200000 | (cc << 28) | (dest << 16) | (src << 12) | -offset);}
static inline uint32_t str_offset_preindex(uint8_t dest, uint8_t src, int16_t offset){return str_cc_offset_preindex(ARM_CC_AL, dest, src, offset);}
static inline uint32_t str_cc_offset_postindex(uint8_t cc, uint8_t dest, uint8_t src, int16_t offset){return (offset >= 0) ? INSN_TO_LE(0x04800000 | (cc << 28) | (dest << 16) | (src << 12) | offset) : INSN_TO_LE(0x04000000 | (cc << 28) | (dest << 16) | (src << 12) | -offset);}
static inline uint32_t str_offset_postindex(uint8_t dest, uint8_t src, int16_t offset){return str_cc_offset_postindex(ARM_CC_AL, dest, src, offset);}
static inline uint32_t strb_cc_offset(uint8_t cc, uint8_t dest, uint8_t src, int16_t offset){return (offset >= 0) ? INSN_TO_LE(0x05c00000 | (cc << 28) | (dest << 16) | (src << 12) | offset) : INSN_TO_LE(0x05400000 | (cc << 28) | (dest << 16) | (src << 12) | -offset);}
static inline uint32_t strb_offset(uint8_t dest, uint8_t src, int16_t offset){return strb_cc_offset(ARM_CC_AL, dest, src, offset);}
static inline uint32_t strb_cc_offset_preindex(uint8_t cc, uint8_t dest, uint8_t src, int16_t offset) {return (offset >= 0) ? INSN_TO_LE(0x05e00000 | (cc << 28) | (dest << 16) | (src << 12) | offset) : INSN_TO_LE(0x05600000 | (cc << 28) | (dest << 16) | (src << 12) | -offset);}
static inline uint32_t strb_offset_preindex(uint8_t dest, uint8_t src, int16_t offset){return strb_cc_offset_preindex(ARM_CC_AL, dest, src, offset);}
static inline uint32_t strb_cc_offset_postindex(uint8_t cc, uint8_t dest, uint8_t src, int16_t offset){return (offset >= 0) ? INSN_TO_LE(0x04c00000 | (cc << 28) | (dest << 16) | (src << 12) | offset) : INSN_TO_LE(0x04400000 | (cc << 28) | (dest << 16) | (src << 12) | -offset);}
static inline uint32_t strb_offset_postindex(uint8_t dest, uint8_t src, int16_t offset){return strb_cc_offset_postindex(ARM_CC_AL, dest, src, offset);}
static inline uint32_t strb_cc_regoffset(uint8_t cc, uint8_t dest, uint8_t src, uint8_t offset, uint8_t shift){return INSN_TO_LE(0x07c00000 | (cc << 28) | (dest << 16) | (src << 12) | offset | ((shift & 0x1f) << 7));}
static inline uint32_t strb_regoffset(uint8_t dest, uint8_t src, uint8_t offset, uint8_t shift){return strb_cc_regoffset(ARM_CC_AL, dest, src, offset, shift);}
static inline uint32_t strex_cc(uint8_t cc, uint8_t rn, uint8_t rm, uint8_t rd) { return INSN_TO_LE(0x01800f90 | (cc << 28) | (rn << 16) | (rd << 12) | rm); }
static inline uint32_t strex(uint8_t rn, uint8_t rm, uint8_t rd) { return strex_cc(ARM_CC_AL, rn, rm, rd); }
static inline uint32_t strexb_cc(uint8_t cc, uint8_t rn, uint8_t rm, uint8_t rd) { return INSN_TO_LE(0x01c00f90 | (cc << 28) | (rn << 16) | (rd << 12) | rm); }
static inline uint32_t strexb(uint8_t rn, uint8_t rm, uint8_t rd) { return strexb_cc(ARM_CC_AL, rn, rm, rd); }
static inline uint32_t strexh_cc(uint8_t cc, uint8_t rn, uint8_t rm, uint8_t rd) { return INSN_TO_LE(0x01e00f90 | (cc << 28) | (rn << 16) | (rd << 12) | rm); }
static inline uint32_t strexh(uint8_t rn, uint8_t rm, uint8_t rd) { return strexh_cc(ARM_CC_AL, rn, rm, rd); }
static inline uint32_t strh_cc_offset(uint8_t cc, uint8_t dest, uint8_t src, int16_t offset){return (offset >= 0) ? INSN_TO_LE(0x01c000b0 | (cc << 28) | (dest << 16) | (src << 12) | (offset & 0x0f) | ((offset << 4) & 0xf00)) : INSN_TO_LE(0x014000b0 | (cc << 28) | (dest << 16) | (src << 12) | ((-offset) & 0x0f) | (((-offset) << 4) & 0xf00));}
static inline uint32_t strh_offset(uint8_t dest, uint8_t src, int16_t offset){return strh_cc_offset(ARM_CC_AL, dest, src, offset);}
static inline uint32_t strh_cc_offset_preindex(uint8_t cc, uint8_t dest, uint8_t src, int8_t offset){return (offset >= 0) ? INSN_TO_LE(0x01e000b0 | (cc << 28) | (dest << 16) | (src << 12) | (offset & 0x0f) | ((offset << 4) & 0xf00)) : INSN_TO_LE(0x016000b0 | (cc << 28) | (dest << 16) | (src << 12) | ((-offset) & 0x0f) | (((-offset) << 4) & 0xf00));}
static inline uint32_t strh_offset_preindex(uint8_t dest, uint8_t src, int8_t offset){return strh_cc_offset_preindex(ARM_CC_AL, dest, src, offset);}
static inline uint32_t strh_cc_offset_postindex(uint8_t cc, uint8_t dest, uint8_t src, int8_t offset){return (offset >= 0) ? INSN_TO_LE(0x00c000b0 | (cc << 28) | (dest << 16) | (src << 12) | (offset & 0x0f) | ((offset << 4) & 0xf00)) : INSN_TO_LE(0x004000b0 | (cc << 28) | (dest << 16) | (src << 12) | ((-offset) & 0x0f) | (((-offset) << 4) & 0xf00));}
static inline uint32_t strh_offset_postindex(uint8_t dest, uint8_t src, int8_t offset){return strh_cc_offset_postindex(ARM_CC_AL, dest, src, offset);}
static inline uint32_t strh_cc_regoffset(uint8_t cc, uint8_t dest, uint8_t src, uint8_t offset){return INSN_TO_LE(0x018000b0 | (cc << 28) | (dest << 16) | (src << 12) | (offset & 0x0f));}
static inline uint32_t strh_regoffset(uint8_t dest, uint8_t src, uint8_t offset){return strh_cc_regoffset(ARM_CC_AL, dest, src, offset);}
static inline uint32_t sbc_cc_immed(uint8_t cc, uint8_t dest, uint8_t src, uint16_t value){dest = dest & 15;src = src & 15;return INSN_TO_LE(0x02c00000 | (cc << 28) | (dest << 12) | (src << 16) | value);}
static inline uint32_t sbc_immed(uint8_t dest, uint8_t src, uint16_t value){return sbc_cc_immed(ARM_CC_AL, dest, src, value);}
static inline uint32_t sbc_cc_reg(uint8_t cc, uint8_t dest, uint8_t src, uint8_t reg, uint8_t lsl){dest = dest & 15;src = src & 15;reg = reg & 15;lsl = lsl & 31;return INSN_TO_LE(0x00c00000 | (cc << 28) | (dest << 12) | (src << 16) | reg | (lsl << 7));}
static inline uint32_t sbc_reg(uint8_t dest, uint8_t src, uint8_t reg, uint8_t lsl){return sbc_cc_reg(ARM_CC_AL, dest, src, reg, lsl);}
static inline uint32_t sbcs_cc_immed(uint8_t cc, uint8_t dest, uint8_t src, uint16_t value){dest = dest & 15;src = src & 15;return INSN_TO_LE(0x02c00000 | (1 << 20) | (cc << 28) | (dest << 12) | (src << 16) | value);}
static inline uint32_t sbcs_immed(uint8_t dest, uint8_t src, uint16_t value){return sbcs_cc_immed(ARM_CC_AL, dest, src, value);}
static inline uint32_t sbcs_cc_reg(uint8_t cc, uint8_t dest, uint8_t src, uint8_t reg, uint8_t lsl){dest = dest & 15;src = src & 15;reg = reg & 15;lsl = lsl & 31;return INSN_TO_LE(0x00c00000 | (1 << 20) | (cc << 28) | (dest << 12) | (src << 16) | reg | (lsl << 7));}
static inline uint32_t sbcs_reg(uint8_t dest, uint8_t src, uint8_t reg, uint8_t lsl){return sbcs_cc_reg(ARM_CC_AL, dest, src, reg, lsl);}
static inline uint32_t sub_cc_immed(uint8_t cc, uint8_t dest, uint8_t src, uint16_t value){dest = dest & 15;src = src & 15;return INSN_TO_LE(0x02400000 | (cc << 28) | (dest << 12) | (src << 16) | value);}
static inline uint32_t sub_immed(uint8_t dest, uint8_t src, uint16_t value){return sub_cc_immed(ARM_CC_AL, dest, src, value);}
static inline uint32_t sub_cc_reg(uint8_t cc, uint8_t dest, uint8_t src, uint8_t reg, uint8_t lsl){dest = dest & 15;src = src & 15;reg = reg & 15;lsl = lsl & 31;return INSN_TO_LE(0x00400000 | (cc << 28) | (dest << 12) | (src << 16) | reg | (lsl << 7));}
static inline uint32_t sub_reg(uint8_t dest, uint8_t src, uint8_t reg, uint8_t lsl){return sub_cc_reg(ARM_CC_AL, dest, src, reg, lsl);}
static inline uint32_t subs_cc_immed(uint8_t cc, uint8_t dest, uint8_t src, uint16_t value){dest = dest & 15;src = src & 15;return INSN_TO_LE(0x02400000 | (1 << 20) | (cc << 28) | (dest << 12) | (src << 16) | value);}
static inline uint32_t subs_immed(uint8_t dest, uint8_t src, uint16_t value){return subs_cc_immed(ARM_CC_AL, dest, src, value);}
static inline uint32_t subs_cc_reg(uint8_t cc, uint8_t dest, uint8_t src, uint8_t reg, uint8_t lsl){dest = dest & 15;src = src & 15;reg = reg & 15;lsl = lsl & 31;return INSN_TO_LE(0x00400000 | (1 << 20) | (cc << 28) | (dest << 12) | (src << 16) | reg | (lsl << 7));}
static inline uint32_t subs_reg(uint8_t dest, uint8_t src, uint8_t reg, uint8_t lsl){return subs_cc_reg(ARM_CC_AL, dest, src, reg, lsl);}
static inline uint32_t sxtb_cc(uint8_t cc, uint8_t dest, uint8_t src, uint8_t rot){return INSN_TO_LE(0x06af0070 | (cc << 28) | (dest << 12) | (src) | (rot << 10));}
static inline uint32_t sxtb(uint8_t dest, uint8_t src, uint8_t rot){return sxtb_cc(ARM_CC_AL, dest, src, rot);}
static inline uint32_t sxth_cc(uint8_t cc, uint8_t dest, uint8_t src, uint8_t rot){return INSN_TO_LE(0x06bf0070 | (cc << 28) | (dest << 12) | (src) | (rot << 10));}
static inline uint32_t sxth(uint8_t dest, uint8_t src, uint8_t rot) {return sxth_cc(ARM_CC_AL, dest, src, rot);}
static inline uint32_t teq_cc_immed(uint8_t cc, uint8_t src, uint16_t value){src = src & 15;return INSN_TO_LE(0x03300000 | (cc << 28) | (src << 16) | value);}
static inline uint32_t teq_immed(uint8_t src, uint16_t value){return teq_cc_immed(ARM_CC_AL, src, value);}
static inline uint32_t teq_cc_reg(uint8_t cc, uint8_t src, uint8_t reg, uint8_t lsl){src = src & 15;reg = reg & 15;lsl = lsl & 31;return INSN_TO_LE(0x01300000 | (cc << 28) | (src << 16) | reg | (lsl << 7));}
static inline uint32_t teq_reg(uint8_t src, uint8_t reg, uint8_t lsl){return teq_cc_reg(ARM_CC_AL, src, reg, lsl);}
static inline uint32_t tst_cc_immed(uint8_t cc, uint8_t src, uint16_t value){src = src & 15;return INSN_TO_LE(0x03100000 | (cc << 28) | (src << 16) | value);}
static inline uint32_t tst_immed(uint8_t src, uint16_t value){return tst_cc_immed(ARM_CC_AL, src, value);}
static inline uint32_t tst_cc_reg(uint8_t cc, uint8_t src, uint8_t reg, uint8_t lsl){src = src & 15;reg = reg & 15;lsl = lsl & 31;return INSN_TO_LE(0x01100000 | (cc << 28) | (src << 16) | reg | (lsl << 7));}
static inline uint32_t tst_reg(uint8_t src, uint8_t reg, uint8_t lsl){return tst_cc_reg(ARM_CC_AL, src, reg, lsl);}
static inline uint32_t ubfx_cc(uint8_t cc, uint8_t reg, uint8_t src, uint8_t lsb, uint8_t width) { return INSN_TO_LE(0x07e00050 | (cc << 28) | (reg << 12) | (lsb << 7) | ((width - 1) << 16) | src); }
static inline uint32_t ubfx(uint8_t reg, uint8_t src, uint8_t lsb, uint8_t width) { return ubfx_cc(ARM_CC_AL, reg, src, lsb, width); }
static inline uint32_t udf(uint16_t immed) { return INSN_TO_LE(0xe7f000f0 | (immed & 0x0f) | ((immed & 0xfff0) << 4)); }
static inline uint32_t umull_cc(uint8_t cc, uint8_t rdhi, uint8_t rdlo, uint8_t rm, uint8_t rs) { return INSN_TO_LE(0x00800090 | (cc << 28) | (rdhi << 16) | (rdlo << 12) | (rs << 8) | rm );}
static inline uint32_t umull(uint8_t rdhi, uint8_t rdlo, uint8_t rm, uint8_t rs) { return umull_cc(ARM_CC_AL, rdhi, rdlo, rm, rs);}
static inline uint32_t umulls_cc(uint8_t cc, uint8_t rdhi, uint8_t rdlo, uint8_t rm, uint8_t rs) { return INSN_TO_LE(0x00800090 | (1 << 20) | (cc << 28) | (rdhi << 16) | (rdlo << 12) | (rs << 8) | rm );}
static inline uint32_t umulls(uint8_t rdhi, uint8_t rdlo, uint8_t rm, uint8_t rs) { return umulls_cc(ARM_CC_AL, rdhi, rdlo, rm, rs);}
static inline uint32_t uxtab_cc(uint8_t cc, uint8_t dest, uint8_t second, uint8_t third, uint8_t rot){return INSN_TO_LE(0x06e00070 | (cc << 28) | (dest << 12) | (second << 16) | (third) | (rot << 10));}
static inline uint32_t uxtab(uint8_t dest, uint8_t second, uint8_t third, uint8_t rot){return uxtab_cc(ARM_CC_AL, dest, second, third, rot);}
static inline uint32_t uxtah_cc(uint8_t cc, uint8_t dest, uint8_t second, uint8_t third, uint8_t rot){return INSN_TO_LE(0x06f00070 | (cc << 28) | (dest << 12) | (second << 16) | (third) | (rot << 10));}
static inline uint32_t uxtah(uint8_t dest, uint8_t second, uint8_t third, uint8_t rot) {return uxtah_cc(ARM_CC_AL, dest, second, third, rot);}
static inline uint32_t uxtb_cc(uint8_t cc, uint8_t dest, uint8_t src, uint8_t rot){return INSN_TO_LE(0x06ef0070 | (cc << 28) | (dest << 12) | (src) | (rot << 10));}
static inline uint32_t uxtb(uint8_t dest, uint8_t src, uint8_t rot){return uxtb_cc(ARM_CC_AL, dest, src, rot);}
static inline uint32_t uxth_cc(uint8_t cc, uint8_t dest, uint8_t src, uint8_t rot){return INSN_TO_LE(0x06ff0070 | (cc << 28) | (dest << 12) | (src) | (rot << 10));}
static inline uint32_t uxth(uint8_t dest, uint8_t src, uint8_t rot) {return uxth_cc(ARM_CC_AL, dest, src, rot);}


static inline uint32_t smlal_cc(uint8_t cc, uint8_t rdhi, uint8_t rdlo, uint8_t factor1, uint8_t factor2) { return INSN_TO_LE(0x00e00090 | (cc << 28) | (rdhi << 16) | (rdlo << 12) | (factor1 << 8) | factor2); }
static inline uint32_t smlal(uint8_t rdhi, uint8_t rdlo, uint8_t factor1, uint8_t factor2) { return smlal_cc(ARM_CC_AL, rdhi, rdlo, factor1, factor2); }
static inline uint32_t umlal_cc(uint8_t cc, uint8_t rdhi, uint8_t rdlo, uint8_t factor1, uint8_t factor2) { return INSN_TO_LE(0x00a00090 | (cc << 28) | (rdhi << 16) | (rdlo << 12) | (factor1 << 8) | factor2); }
static inline uint32_t umlal(uint8_t rdhi, uint8_t rdlo, uint8_t factor1, uint8_t factor2) { return umlal_cc(ARM_CC_AL, rdhi, rdlo, factor1, factor2); }
static inline uint32_t mla_cc(uint8_t cc, uint8_t dest, uint8_t src, uint8_t factor1, uint8_t factor2) { return INSN_TO_LE(0x00200090 | (cc << 28) | (dest << 16) | (src << 12) | (factor1 << 8) | factor2); }
static inline uint32_t mla(uint8_t dest, uint8_t src, uint8_t factor1, uint8_t factor2) { return mla_cc(ARM_CC_AL, dest, src, factor1, factor2); }
static inline uint32_t mls_cc(uint8_t cc, uint8_t dest, uint8_t src, uint8_t factor1, uint8_t factor2) { return INSN_TO_LE(0x00600090 | (cc << 28) | (dest << 16) | (src << 12) | (factor1 << 8) | factor2); }
static inline uint32_t mls(uint8_t dest, uint8_t src, uint8_t factor1, uint8_t factor2) { return mls_cc(ARM_CC_AL, dest, src, factor1, factor2); }
static inline uint32_t sdiv_cc(uint8_t cc, uint8_t dest, uint8_t dividend, uint8_t divisor) { return INSN_TO_LE(0x0710f010 | (cc << 28) | (dest << 16) | (divisor << 8) | dividend); }
static inline uint32_t sdiv(uint8_t dest, uint8_t dividend, uint8_t divisor) { return sdiv_cc(ARM_CC_AL, dest, dividend, divisor); }
static inline uint32_t udiv_cc(uint8_t cc, uint8_t dest, uint8_t dividend, uint8_t divisor) { return INSN_TO_LE(0x0730f010 | (cc << 28) | (dest << 16) | (divisor << 8) | dividend); }
static inline uint32_t udiv(uint8_t dest, uint8_t dividend, uint8_t divisor) { return udiv_cc(ARM_CC_AL, dest, dividend, divisor); }


/* VFP */
static inline uint32_t fabsd_cc(uint8_t cc, uint8_t v_dst, uint8_t v_src) { return INSN_TO_LE(0x0eb00bc0 | (cc << 28) | (v_dst << 12) | (v_src)); }
static inline uint32_t fabsd(uint8_t v_dst, uint8_t v_src) { return fabsd_cc(ARM_CC_AL, v_dst, v_src); }
static inline uint32_t fabss_cc(uint8_t cc, uint8_t v_dst, uint8_t v_src) { return INSN_TO_LE(0x0eb00ac0 | (cc << 28) | ((v_dst >> 1) << 12) | (v_src >> 1) | ((v_dst & 1) << 22) | ((v_src & 1) << 5)); }
static inline uint32_t fabss(uint8_t v_dst, uint8_t v_src) { return fabss_cc(ARM_CC_AL, v_dst, v_src); }
static inline uint32_t faddd_cc(uint8_t cc, uint8_t v_dst, uint8_t v_first, uint8_t v_second) { return INSN_TO_LE(0x0e300b00 | (cc << 28) | (v_dst << 12) | (v_first << 16) | (v_second));}
static inline uint32_t faddd(uint8_t v_dst, uint8_t v_first, uint8_t v_second) { return faddd_cc(ARM_CC_AL, v_dst, v_first, v_second); }
static inline uint32_t fcmpd_cc(uint8_t cc, uint8_t v_dst, uint8_t v_src) { return INSN_TO_LE(0x0eb40b40 | (cc << 28) | (v_dst << 12) | (v_src)); }
static inline uint32_t fcmpd(uint8_t v_dst, uint8_t v_src) { return fcmpd_cc(ARM_CC_AL, v_dst, v_src); }
static inline uint32_t fcmpzd_cc(uint8_t cc, uint8_t v_src) { return INSN_TO_LE(0x0eb50b40 | (cc << 28) |  (v_src << 12)); }
static inline uint32_t fcmpzd(uint8_t v_src) { return fcmpzd_cc(ARM_CC_AL, v_src); }
static inline uint32_t fcpyd_cc(uint8_t cc, uint8_t v_dst, uint8_t v_src) { return INSN_TO_LE(0x0eb00b40 | (cc << 28) | (v_dst << 12) | (v_src)); }
static inline uint32_t fcpyd(uint8_t v_dst, uint8_t v_src) { return fcpyd_cc(ARM_CC_AL, v_dst, v_src); }
static inline uint32_t fcvtds_cc(uint8_t cc, uint8_t d_dst, uint8_t s_src) { return INSN_TO_LE(0x0eb70ac0 | (cc << 28) | (d_dst << 12) | (s_src >> 1) | (s_src & 1 ? 0x20:0)); }
static inline uint32_t fcvtds(uint8_t d_dst, uint8_t s_src) { return fcvtds_cc(ARM_CC_AL, d_dst, s_src); }
static inline uint32_t fcvtsd_cc(uint8_t cc, uint8_t s_dst, uint8_t d_src) { return INSN_TO_LE(0x0eb70bc0 | (cc << 28) | ((s_dst >> 1) << 12) | (d_src) | (s_dst & 1 ? (1 << 22):0)); }
static inline uint32_t fcvtsd(uint8_t s_dst, uint8_t d_src) { return fcvtsd_cc(ARM_CC_AL, s_dst, d_src); }
static inline uint32_t fdivd_cc(uint8_t cc, uint8_t v_dst, uint8_t v_dividend, uint8_t v_divisor) { return INSN_TO_LE(0x0e800b00 | (cc << 28) | (v_dst << 12) | (v_dividend << 16) | (v_divisor));}
static inline uint32_t fdivd(uint8_t v_dst, uint8_t v_dividend, uint8_t v_divisor) { return fdivd_cc(ARM_CC_AL, v_dst, v_dividend, v_divisor); }
static inline uint32_t fldd_cc(uint8_t cc, uint8_t v_dst, uint8_t base, int8_t offset) { return INSN_TO_LE((offset < 0 ? 0x0d100b00:0x0d900b00) | (cc << 28) | (base << 16) | (v_dst << 12) | ((offset < 0) ? -offset : offset)); }
static inline uint32_t fldd(uint8_t v_dst, uint8_t base, int8_t offset) { return fldd_cc(ARM_CC_AL, v_dst, base, offset); }
static inline uint32_t flds_cc(uint8_t cc, uint8_t v_dst, uint8_t base, int8_t offset) { return INSN_TO_LE((offset < 0 ? 0x0d100a00:0x0d900a00) | (cc << 28) | (base << 16) | ((v_dst >> 1) << 12) | ((v_dst & 1) << 22) | ((offset < 0) ? -offset : offset)); }
static inline uint32_t flds(uint8_t v_dst, uint8_t base, int8_t offset) { return flds_cc(ARM_CC_AL, v_dst, base, offset); }
static inline uint32_t fmacd_cc(uint8_t cc, uint8_t v_dst, uint8_t v_first, uint8_t v_second) { return INSN_TO_LE(0x0ea00b00 | (cc << 28) | (v_dst << 12) | (v_first << 16) | (v_second));}
static inline uint32_t fmacd(uint8_t v_dst, uint8_t v_first, uint8_t v_second) { return fmacd_cc(ARM_CC_AL, v_dst, v_first, v_second); }
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

static inline uint32_t fmov_0(uint8_t v_dst) { return fmov_i64(v_dst, 0, 0xe); }
static inline uint32_t fmov_1(uint8_t v_dst) { return fmov_imm(v_dst, 112); }

static inline uint32_t fmrrd_cc(uint8_t cc, uint8_t dst_hi, uint8_t dst_lo, uint8_t v_src) { return INSN_TO_LE(0x0c500b10 | (cc << 28) | (v_src) | (dst_lo << 12) | (dst_hi << 16)); }
static inline uint32_t fmrrd(uint8_t dst_hi, uint8_t dst_lo, uint8_t v_src) { return fmrrd_cc(ARM_CC_AL, dst_hi, dst_lo, v_src); }
static inline uint32_t fmrs_cc(uint8_t cc, uint8_t dst, uint8_t v_src) { return INSN_TO_LE(0x0e100a10 | (cc << 28) | ((v_src >> 1) << 16) | (dst << 12) | ((v_src & 1) << 7)); }
static inline uint32_t fmrs(uint8_t dst, uint8_t v_src) { return fmrs_cc(ARM_CC_AL, dst, v_src); }
static inline uint32_t fmstat_cc(uint8_t cc) { return INSN_TO_LE(0x0ef1fa10 | (cc << 28)); }
static inline uint32_t fmstat() { return fmstat_cc(ARM_CC_AL); }
static inline uint32_t fmuld_cc(uint8_t cc, uint8_t v_dst, uint8_t v_first, uint8_t v_second) { return INSN_TO_LE(0x0e200b00 | (cc << 28) | (v_dst << 12) | (v_first << 16) | (v_second));}
static inline uint32_t fmuld(uint8_t v_dst, uint8_t v_first, uint8_t v_second) { return fmuld_cc(ARM_CC_AL, v_dst, v_first, v_second); }
static inline uint32_t fnegd_cc(uint8_t cc, uint8_t v_dst, uint8_t v_src) { return INSN_TO_LE(0x0eb10b40 | (cc << 28) | (v_dst << 12) | (v_src)); }
static inline uint32_t fnegd(uint8_t v_dst, uint8_t v_src) { return fnegd_cc(ARM_CC_AL, v_dst, v_src); }
static inline uint32_t fsitod_cc(uint8_t cc, uint8_t d_dst, uint8_t s_src) { return INSN_TO_LE(0x0eb80bc0 | (cc << 28) | (d_dst << 12) | (s_src >> 1) | (s_src & 1 ? 0x20:0)); }
static inline uint32_t fsitod(uint8_t d_dst, uint8_t s_src) { return fsitod_cc(ARM_CC_AL, d_dst, s_src); }
static inline uint32_t fstd_cc(uint8_t cc, uint8_t v_dst, uint8_t base, int8_t offset) { return INSN_TO_LE((offset < 0 ? 0x0d000b00:0x0d800b00) | (cc << 28) | (base << 16) | (v_dst << 12) | ((offset < 0) ? -offset : offset)); }
static inline uint32_t fstd(uint8_t v_dst, uint8_t base, int8_t offset) { return fstd_cc(ARM_CC_AL, v_dst, base, offset); }
static inline uint32_t fsts_cc(uint8_t cc, uint8_t v_dst, uint8_t base, int8_t offset) { return INSN_TO_LE((offset < 0 ? 0x0d000a00:0x0d800a00) | (cc << 28) | (base << 16) | ((v_dst >> 1) << 12) | ((v_dst & 1) << 22) | ((offset < 0) ? -offset : offset)); }
static inline uint32_t fsts(uint8_t v_dst, uint8_t base, int8_t offset) { return fsts_cc(ARM_CC_AL, v_dst, base, offset); }
static inline uint32_t fsqrtd_cc(uint8_t cc, uint8_t v_dst, uint8_t v_src) { return INSN_TO_LE(0x0eb10bc0 | (cc << 28) | (v_dst << 12) | (v_src)); }
static inline uint32_t fsqrtd(uint8_t v_dst, uint8_t v_src) { return fsqrtd_cc(ARM_CC_AL, v_dst, v_src); }
static inline uint32_t fsubd_cc(uint8_t cc, uint8_t v_dst, uint8_t v_first, uint8_t v_second) { return INSN_TO_LE(0x0e300b40 | (cc << 28) | (v_dst << 12) | (v_first << 16) | (v_second));}
static inline uint32_t fsubd(uint8_t v_dst, uint8_t v_first, uint8_t v_second) { return fsubd_cc(ARM_CC_AL, v_dst, v_first, v_second); }
static inline uint32_t ftosid_cc(uint8_t cc, uint8_t s_dst, uint8_t d_src) { return INSN_TO_LE(0x0ebd0b40 | (cc << 28) | ((s_dst >> 1) << 12) | d_src | (s_dst & 1 ? (1 << 22):0)); }
static inline uint32_t ftosid(uint8_t s_dst, uint8_t d_src) { return ftosid_cc(ARM_CC_AL, s_dst, d_src); }
static inline uint32_t ftosidrz_cc(uint8_t cc, uint8_t s_dst, uint8_t d_src) { return INSN_TO_LE(0x0ebd0bc0 | (cc << 28) | ((s_dst >> 1) << 12) | d_src | (s_dst & 1 ? (1 << 22):0)); }
static inline uint32_t ftosidrz(uint8_t s_dst, uint8_t d_src) { return ftosidrz_cc(ARM_CC_AL, s_dst, d_src); }


#include <RegisterAllocator.h>

static inline __attribute__((always_inline))
uint32_t * EMIT_GetFPUFlags(uint32_t * ptr, uint8_t fpsr)
{
    *ptr++ = fmstat();
    *ptr++ = bic_immed(fpsr, fpsr, 0x40f);
    *ptr++ = orr_cc_immed(ARM_CC_EQ, fpsr, fpsr, 0x404);
    *ptr++ = orr_cc_immed(ARM_CC_MI, fpsr, fpsr, 0x408);
    *ptr++ = orr_cc_immed(ARM_CC_VS, fpsr, fpsr, 0x401);

    return ptr;
}

static inline __attribute__((always_inline))
uint32_t * EMIT_ClearFlags(uint32_t * ptr, uint8_t cc, uint8_t flags)
{
    *ptr++ = bic_immed(cc, cc, flags);

    return ptr;
}

static inline __attribute__((always_inline))
uint32_t * EMIT_GetNZ00(uint32_t * ptr, uint8_t cc, uint8_t *not_done)
{
    (void)cc;
    ptr = EMIT_ClearFlags(ptr, cc, 15);
    (*not_done) &= 0x1f;
    return ptr;
}

static inline __attribute__((always_inline))
uint32_t * EMIT_GetNZxx(uint32_t * ptr, uint8_t cc, uint8_t *not_done)
{
    (void)cc;
    ptr = EMIT_ClearFlags(ptr, cc, 12);
    (*not_done) &= 0x1f;
    return ptr;
}

static inline __attribute__((always_inline))
uint32_t * EMIT_GetNZVC(uint32_t * ptr, uint8_t cc, uint8_t *not_done)
{
    (void)cc;
    ptr = EMIT_ClearFlags(ptr, cc, 15);
    (*not_done) &= 0x1f;
    return ptr;
}

static inline __attribute__((always_inline))
uint32_t * EMIT_GetNZVCX(uint32_t * ptr, uint8_t cc, uint8_t *not_done)
{
    (void)cc;
    ptr = EMIT_ClearFlags(ptr, cc, 31);
    (*not_done) &= 0x1f;
    return ptr;
}

static inline __attribute__((always_inline))
uint32_t * EMIT_GetNZVnC(uint32_t * ptr, uint8_t cc, uint8_t *not_done)
{
    (void)cc;
    ptr = EMIT_ClearFlags(ptr, cc, 15);
    (*not_done) &= 0x1f;
    return ptr;
}

static inline __attribute__((always_inline))
uint32_t * EMIT_GetNZVnCX(uint32_t * ptr, uint8_t cc, uint8_t *not_done)
{
    (void)cc;
    ptr = EMIT_ClearFlags(ptr, cc, 31);
    (*not_done) &= 0x1f;
    return ptr;
}

static inline __attribute__((always_inline))
uint32_t * EMIT_SetFlags(uint32_t * ptr, uint8_t cc, uint8_t flags)
{
    *ptr++ = orr_immed(cc, cc, flags);

    return ptr;
}

static inline __attribute__((always_inline))
uint32_t * EMIT_SetFlagsConditional(uint32_t * ptr, uint8_t cc, uint8_t flags, uint8_t cond)
{
    *ptr++ = orr_cc_immed(cond, cc, cc, flags);

    return ptr;
}

#endif /* _ARM_H */
